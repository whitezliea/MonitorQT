#include "application/ApplicationLayer.h"
#include "application/abstractions/IRawFrameSource.h"
#include "application/events/EventBus.h"
#include "application/pipelines/DataCleanPipeline.h"
#include "application/queues/BlockingQueue.h"
#include "application/runtime/MonitoringRuntimeService.h"
#include "application/runtime/PersistenceRuntimeCoordinator.h"
#include "application/services/AlarmService.h"
#include "application/services/OperationLogService.h"
#include "application/services/RuntimeEventConsumers.h"
#include "application/services/TagDefinitionCatalog.h"
#include "application/services/UiSnapshotProvider.h"
#include "application/workers/BatchPersistWorker.h"
#include "domain/DomainLayer.h"
#include "domain/common/DomainCommon.h"
#include "domain/devices/DeviceModels.h"
#include "domain/measurements/MeasurementModels.h"
#include "domain/tags/TagModels.h"
#include "infrastructure/InfrastructureLayer.h"
#include "infrastructure/persistence/SQLiteAlarmRepository.h"
#include "infrastructure/persistence/SQLiteConfigurationRepository.h"
#include "infrastructure/persistence/SQLiteHistoryRepository.h"
#include "infrastructure/persistence/SQLiteOperationLogRepository.h"
#include "infrastructure/persistence/SqliteConnectionFactory.h"
#include "presentation/PresentationLayer.h"
#include "presentation/export/CsvExportWriter.h"
#include "simulator/SimulatorLayer.h"

#include <QCoreApplication>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace {

using Monitor::Application::Pipelines::DataCleanPipeline;
using Monitor::Application::Runtime::MonitoringRuntimeService;
using Monitor::Application::Services::AlarmService;
using Monitor::Application::Services::AlarmEventConsumer;
using Monitor::Application::Services::AlarmOperationLogConsumer;
using Monitor::Application::Services::OperationLogService;
using Monitor::Application::Services::TagDefinitionCatalog;
using Monitor::Application::Workers::BatchPersistWorker;
using Monitor::Application::Runtime::PersistenceRuntimeCoordinator;
using Monitor::Domain::Alarms::AlarmEvent;
using Monitor::Domain::Alarms::AlarmLevel;
using Monitor::Domain::Alarms::AlarmState;
using Monitor::Domain::Common::UtcDateTime;
using Monitor::Domain::Devices::DeviceStatus;
using Monitor::Domain::Measurements::ChannelValue;
using Monitor::Domain::Measurements::MatrixFrame;
using Monitor::Domain::Measurements::RawMeasurementFrame;
using Monitor::Domain::Tags::CleanedTagValue;
using Monitor::Domain::Tags::TagAlarmState;
using Monitor::Domain::Tags::TagQuality;
using Monitor::Domain::Tags::TagValue;
using Monitor::Infrastructure::Persistence::AlarmQuery;
using Monitor::Infrastructure::Persistence::AlarmSortDirection;
using Monitor::Infrastructure::Persistence::HistoryQuery;
using Monitor::Infrastructure::Persistence::HistorySortDirection;
using Monitor::Infrastructure::Persistence::SQLiteAlarmRepository;
using Monitor::Infrastructure::Persistence::SQLiteConfigurationRepository;
using Monitor::Infrastructure::Persistence::SQLiteHistoryRepository;
using Monitor::Infrastructure::Persistence::SQLiteOperationLogRepository;
using Monitor::Infrastructure::Persistence::SqliteConnectionFactory;

class TestFailure final : public std::runtime_error
{
public:
    explicit TestFailure(const QString &message)
        : std::runtime_error(message.toStdString())
    {
    }
};

class NoopRawFrameSource final : public Monitor::Application::Abstractions::IRawFrameSource
{
public:
    bool readNextFrame(
        const QDateTime &,
        Monitor::Domain::Measurements::RawMeasurementFrame *) override
    {
        return false;
    }

    void cancel() override
    {
        m_canceled = true;
    }

    void resetCancellation() override
    {
        m_canceled = false;
    }

private:
    bool m_canceled = false;
};

void expect(bool condition, const QString &message)
{
    if (!condition) {
        throw TestFailure(message);
    }
}

void expectNear(double actual, double expected, double tolerance, const QString &message)
{
    if (std::abs(actual - expected) > tolerance) {
        throw TestFailure(QStringLiteral("%1 Actual=%2 Expected=%3").arg(message).arg(actual).arg(expected));
    }
}

QDateTime utcTime(qint64 milliseconds)
{
    return UtcDateTime::fromCSharpTicks(
        UtcDateTime::CSharpTicksAtUnixEpoch + milliseconds * UtcDateTime::TicksPerMillisecond);
}

const CleanedTagValue *findValue(const QVector<CleanedTagValue> &values, const QString &tagId)
{
    const auto it = std::find_if(values.cbegin(), values.cend(), [&tagId](const auto &value) {
        return value.tagId == tagId;
    });
    return it == values.cend() ? nullptr : &(*it);
}

const AlarmEvent *findAlarm(const QVector<AlarmEvent> &alarms, const QString &tagId)
{
    const auto it = std::find_if(alarms.cbegin(), alarms.cend(), [&tagId](const auto &alarm) {
        return alarm.tagId == tagId;
    });
    return it == alarms.cend() ? nullptr : &(*it);
}

RawMeasurementFrame createFrame(qint64 sequenceNo, const QDateTime &timestampUtc)
{
    RawMeasurementFrame frame;
    frame.frameId = QUuid::createUuid();
    frame.deviceId = TagDefinitionCatalog::defaultDeviceId();
    frame.sequenceNo = sequenceNo;
    frame.timestampUtc = timestampUtc;
    frame.deviceStatus = DeviceStatus::Running;
    frame.channelValues = {
        ChannelValue{QStringLiteral("TEMP_CH01"), 25.0, QStringLiteral("C"), TagQuality::Good, 0},
        ChannelValue{QStringLiteral("PRESSURE_CH01"), 101.3, QStringLiteral("kPa"), TagQuality::Good, 0},
        ChannelValue{QStringLiteral("LIGHT_CH01"), 800.0, QStringLiteral("lux"), TagQuality::Good, 0},
        ChannelValue{QStringLiteral("VOLTAGE_CH01"), 12.0, QStringLiteral("V"), TagQuality::Good, 0},
        ChannelValue{QStringLiteral("CURRENT_CH01"), 2.0, QStringLiteral("A"), TagQuality::Good, 0},
        ChannelValue{QStringLiteral("VIBRATION_CH01"), 0.7, QStringLiteral("mm/s"), TagQuality::Good, 0}
    };

    QVector<QVector<double>> rows;
    rows.reserve(16);
    for (auto row = 0; row < 16; ++row) {
        QVector<double> values;
        values.reserve(16);
        for (auto column = 0; column < 16; ++column) {
            values.append(100.0 + row * 10.0 + column);
        }
        rows.append(values);
    }
    rows[9][10] = 2000.0;

    frame.matrixValues = MatrixFrame::fromRows(QUuid::createUuid(), timestampUtc, rows, frame.frameId, sequenceNo);
    frame.errorCode = 0;
    frame.quality = TagQuality::Good;
    return frame;
}

CleanedTagValue cleanedNumeric(
    const QString &tagId,
    double value,
    const QDateTime &timestampUtc,
    TagQuality quality = TagQuality::Good)
{
    CleanedTagValue result;
    result.tagId = tagId;
    result.numericValue = value;
    result.timestampUtc = timestampUtc;
    result.quality = quality;
    result.sourceDeviceId = TagDefinitionCatalog::defaultDeviceId();
    result.sourceFrameId = QUuid::createUuid();
    result.sequenceNo = 1;
    return result;
}

void runLayerValidationTests()
{
    auto assertNoErrors = [](const QString &name, const QStringList &errors) {
        expect(errors.isEmpty(), QStringLiteral("%1 validation failed: %2").arg(name, errors.join(QStringLiteral("; "))));
    };

    assertNoErrors(QStringLiteral("Domain"), Monitor::Domain::validateDomainLayer());
    assertNoErrors(QStringLiteral("Application"), Monitor::Application::validateApplicationLayer());
    assertNoErrors(QStringLiteral("Infrastructure"), Monitor::Infrastructure::validateInfrastructureLayer());
    assertNoErrors(QStringLiteral("Simulator"), Monitor::Simulator::validateSimulatorLayer());

    const auto presentation = Monitor::Presentation::presentationLayerInfo();
    expect(presentation.requiredPages.contains(QStringLiteral("Trend")), QStringLiteral("Presentation layer must expose Trend page."));
    expect(presentation.requiredQtModules.contains(QStringLiteral("QtWidgets")), QStringLiteral("Presentation layer must require QtWidgets."));
}

void runMatrixAndPipelineTests()
{
    const auto timestamp = utcTime(1000);
    const auto frame = createFrame(7, timestamp);
    const auto statistics = frame.matrixValues->calculateStatistics();
    expect(statistics.validCount == 256, QStringLiteral("Matrix statistics must count 16x16 valid cells."));
    expect(statistics.maxValue == 2000.0, QStringLiteral("Matrix statistics must include hotspot maximum."));
    expect(frame.matrixValues->valueAt(9, 10) == 2000.0, QStringLiteral("Matrix valueAt must preserve hotspot coordinate."));

    DataCleanPipeline pipeline(
        TagDefinitionCatalog::createDefaults(),
        TagDefinitionCatalog::createSourceMappings());
    const auto values = pipeline.cleanToCleanedValues(frame);
    expect(values.size() >= 20, QStringLiteral("Pipeline must produce the default mapped tag set."));

    const auto *power = findValue(values, QStringLiteral("MEAS.POWER.CH01"));
    expect(power && power->numericValue.has_value(), QStringLiteral("Pipeline must calculate power derived tag."));
    expectNear(power->numericValue.value(), 24.0, 0.0001, QStringLiteral("Power tag must equal voltage * current."));

    const auto *load = findValue(values, QStringLiteral("MEAS.LOAD_RATIO.CH01"));
    expect(load && load->numericValue.has_value(), QStringLiteral("Pipeline must calculate load ratio derived tag."));
    expectNear(load->numericValue.value(), 40.0, 0.0001, QStringLiteral("Load ratio must equal current / 5A."));

    const auto *online = findValue(values, QStringLiteral("DEVICE.ONLINE"));
    expect(online && online->boolValue.value_or(false), QStringLiteral("Pipeline must map running device to online tag."));

    const auto *hotspotRow = findValue(values, QStringLiteral("MATRIX.LIGHT.HOTSPOT_ROW"));
    const auto *hotspotColumn = findValue(values, QStringLiteral("MATRIX.LIGHT.HOTSPOT_COL"));
    expect(hotspotRow && hotspotColumn, QStringLiteral("Pipeline must expose matrix hotspot coordinates."));
    expectNear(hotspotRow->numericValue.value_or(-1), 9.0, 0.0001, QStringLiteral("Hotspot row must be preserved."));
    expectNear(hotspotColumn->numericValue.value_or(-1), 10.0, 0.0001, QStringLiteral("Hotspot column must be preserved."));
}

void runAlarmLifecycleTests()
{
    const auto definitions = TagDefinitionCatalog::createDefaults();
    AlarmService service(definitions);
    const auto first = utcTime(2000);

    const auto raised = service.evaluateWithChanges(
        {cleanedNumeric(QStringLiteral("MEAS.VIBRATION.CH01"), 3.0, first)},
        first);
    const auto currentAfterRaise = service.currentAlarms();
    const auto *raisedAlarm = findAlarm(currentAfterRaise, QStringLiteral("MEAS.VIBRATION.CH01"));
    expect(raisedAlarm, QStringLiteral("AlarmService must raise an active vibration alarm."));
    expect(raisedAlarm->level == AlarmLevel::Alarm, QStringLiteral("Vibration alarm high must be Alarm level."));
    expect(!raised.lifecycleChanges.isEmpty(), QStringLiteral("AlarmService must publish raise lifecycle change."));

    Monitor::Application::Queues::AlarmEventQueue alarmEventQueue;
    Monitor::Application::Queues::OperationLogQueue operationLogQueue;
    OperationLogService operationLogService(&operationLogQueue);
    AlarmEventConsumer alarmEventConsumer(&alarmEventQueue);
    AlarmOperationLogConsumer alarmOperationLogConsumer(&operationLogService);
    Monitor::Application::EventBus eventBus;
    eventBus.registerHandler(
        QStringLiteral("AlarmAcknowledgedEvent"),
        QStringLiteral("AlarmEventConsumer"),
        Monitor::Application::EventHandlerFailurePolicy::Isolated,
        10,
        [&alarmEventConsumer](const Monitor::Application::Events::ApplicationEvent &event) {
            alarmEventConsumer.handle(event);
        });
    eventBus.registerHandler(
        QStringLiteral("AlarmAcknowledgedEvent"),
        QStringLiteral("AlarmOperationLogConsumer"),
        Monitor::Application::EventHandlerFailurePolicy::Isolated,
        20,
        [&alarmOperationLogConsumer](const Monitor::Application::Events::ApplicationEvent &event) {
            alarmOperationLogConsumer.handle(event);
        });

    NoopRawFrameSource source;
    DataCleanPipeline pipeline(definitions, TagDefinitionCatalog::createSourceMappings());
    MonitoringRuntimeService runtime(
        &source,
        &pipeline,
        &service,
        &eventBus,
        Monitor::Application::Configuration::MonitorRuntimeOptions());
    AlarmEvent acknowledged;
    QStringList acknowledgeErrors;
    expect(runtime.acknowledgeAlarm(raisedAlarm->alarmId, first.addMSecs(100), &acknowledged, &acknowledgeErrors),
           QStringLiteral("Active alarm must be acknowledgeable through MonitoringRuntimeService: %1").arg(acknowledgeErrors.join(QStringLiteral("; "))));
    expect(acknowledged.state == AlarmState::Acknowledged, QStringLiteral("Acknowledged alarm state must be persisted in service."));
    expect(alarmEventQueue.size() == 1, QStringLiteral("Acknowledged alarm must be published to the alarm event queue."));
    expect(operationLogQueue.size() == 1, QStringLiteral("Acknowledged alarm must be published to the operation log queue."));

    const auto recovered = service.evaluateWithChanges(
        {cleanedNumeric(QStringLiteral("MEAS.VIBRATION.CH01"), 0.5, first.addMSecs(200))},
        first.addMSecs(200));
    expect(service.currentAlarms().isEmpty(), QStringLiteral("Recovered alarm must leave current alarm set."));
    expect(!recovered.lifecycleChanges.isEmpty(), QStringLiteral("AlarmService must publish recover lifecycle change."));
    const auto history = service.alarmEvents();
    expect(!history.isEmpty() && history.first().state == AlarmState::Recovered, QStringLiteral("Alarm history must keep recovered lifecycle state."));
    expect(history.first().closeReason.has_value(), QStringLiteral("Recovered alarm must keep close reason."));
}

void runQueueAndWorkerTests()
{
    Monitor::Application::Queues::BlockingQueue<int> queue;
    QVector<int> persisted;
    QMutex persistedMutex;

    BatchPersistWorker<int> worker(
        QStringLiteral("History"),
        &queue,
        50,
        2,
        [&persisted, &persistedMutex](const QVector<int> &items) {
            QMutexLocker locker(&persistedMutex);
            persisted += items;
        });

    PersistenceRuntimeCoordinator coordinator({&worker});
    expect(coordinator.start(), QStringLiteral("PersistenceRuntimeCoordinator must start worker."));
    queue.enqueue(1);
    queue.enqueue(2);
    queue.enqueue(3);
    expect(coordinator.flushHistory(), QStringLiteral("Explicit history flush must succeed."));
    expect(coordinator.stop(), QStringLiteral("PersistenceRuntimeCoordinator must stop worker."));

    QMutexLocker locker(&persistedMutex);
    std::sort(persisted.begin(), persisted.end());
    expect(persisted == QVector<int>({1, 2, 3}), QStringLiteral("Worker flush must persist queued items exactly once."));
}

void runSqliteRepositoryTests()
{
    QTemporaryDir directory;
    expect(directory.isValid(), QStringLiteral("Test must create a temporary SQLite directory."));
    const auto databasePath = directory.filePath(QStringLiteral("monitor-test.db"));

    SqliteConnectionFactory factory(databasePath);
    factory.initialize();
    expect(factory.schemaVersion() == SqliteConnectionFactory::currentSchemaVersion(), QStringLiteral("SQLite schema version must be current."));
    expect(factory.journalMode().compare(QStringLiteral("wal"), Qt::CaseInsensitive) == 0, QStringLiteral("SQLite WAL must be enabled."));

    SQLiteHistoryRepository historyRepository(&factory);
    SQLiteAlarmRepository alarmRepository(&factory);
    SQLiteOperationLogRepository logRepository(&factory);
    SQLiteConfigurationRepository configurationRepository(&factory);

    const auto now = utcTime(3000);
    historyRepository.append({
        TagValue{QStringLiteral("TAG.A"), 1.0, now, TagQuality::Good, TagAlarmState::Normal, QStringLiteral("f1"), 1},
        TagValue{QStringLiteral("TAG.A"), 2.0, now.addMSecs(1), TagQuality::Good, TagAlarmState::WarningHigh, QStringLiteral("f2"), 2},
        TagValue{QStringLiteral("TAG.A"), 3.0, now.addMSecs(2), TagQuality::Good, TagAlarmState::AlarmHigh, QStringLiteral("f3"), 3}
    });
    const auto historyPage = historyRepository.query(HistoryQuery{
        QStringLiteral("TAG.A"),
        now.addMSecs(-1),
        now.addMSecs(5),
        1,
        2,
        HistorySortDirection::Descending
    });
    expect(historyPage.items.size() == 2, QStringLiteral("History query must honor page size."));
    expect(historyPage.totalCount == 3, QStringLiteral("History query must report total count."));
    expect(historyPage.hasNextPage(), QStringLiteral("History query must report next page."));
    expectNear(historyPage.items.first().value, 3.0, 0.0001, QStringLiteral("History query must sort descending."));

    const auto alarmId = QUuid::createUuid();
    AlarmEvent alarm;
    alarm.alarmId = alarmId;
    alarm.tagId = QStringLiteral("TAG.A");
    alarm.level = AlarmLevel::Alarm;
    alarm.state = AlarmState::Active;
    alarm.triggerValue = 3.0;
    alarm.triggerTimeUtc = now;
    alarm.message = QStringLiteral("Raised");
    alarm.alarmType = TagAlarmState::AlarmHigh;
    alarm.lastUpdatedTimeUtc = now;
    alarmRepository.append({alarm});
    alarm.state = AlarmState::Recovered;
    alarm.recoverTimeUtc = now.addMSecs(10);
    alarm.closeReason = QStringLiteral("RecoveredToNormal");
    alarm.lastUpdatedTimeUtc = now.addMSecs(10);
    alarmRepository.append({alarm});

    const auto alarmPage = alarmRepository.query(AlarmQuery{
        now.addMSecs(-1),
        now.addMSecs(20),
        QStringLiteral("TAG.A"),
        AlarmLevel::Alarm,
        AlarmState::Recovered,
        1,
        5,
        AlarmSortDirection::Descending
    });
    expect(alarmPage.items.size() == 1 && alarmPage.items.first().alarmId == alarmId, QStringLiteral("Alarm repository must upsert and filter lifecycle events."));
    expect(alarmRepository.queryOpenAlarms().isEmpty(), QStringLiteral("Recovered alarms must not appear in open alarm query."));

    logRepository.append({
        Monitor::Domain::Logs::OperationLog{
            now,
            Monitor::Domain::Logs::OperationLogLevel::Info,
            QStringLiteral("Export"),
            QStringLiteral("CSV exported"),
            QStringLiteral("Export.Csv"),
            QStringLiteral("test"),
            QStringLiteral("detail"),
            QStringLiteral("corr"),
            0
        },
        Monitor::Domain::Logs::OperationLog{
            now.addMSecs(1),
            Monitor::Domain::Logs::OperationLogLevel::Warning,
            QStringLiteral("Alarm"),
            QStringLiteral("Alarm raised"),
            QStringLiteral("Alarm.Raised"),
            QStringLiteral("test"),
            QStringLiteral("detail-2"),
            QStringLiteral("corr-2"),
            0
        },
        Monitor::Domain::Logs::OperationLog{
            now.addMSecs(2),
            Monitor::Domain::Logs::OperationLogLevel::Warning,
            QStringLiteral("Alarm"),
            QStringLiteral("Alarm acknowledged"),
            QStringLiteral("Alarm.Acknowledged"),
            QStringLiteral("test"),
            QStringLiteral("detail-3"),
            QStringLiteral("corr-3"),
            0
        }
    });
    expect(logRepository.queryLatest(1).size() == 1, QStringLiteral("Operation log repository must persist latest log."));
    const auto logPage = logRepository.queryPage(Monitor::Domain::Logs::OperationLogQuery{
        now.addMSecs(-1),
        now.addMSecs(5),
        Monitor::Domain::Logs::OperationLogLevel::Warning,
        QStringLiteral("alarm"),
        20,
        1,
        1
    });
    expect(logPage.items.size() == 1, QStringLiteral("Operation log query page must honor page size."));
    expect(logPage.totalCount == 2, QStringLiteral("Operation log query page must report total count."));
    expect(logPage.hasNextPage(), QStringLiteral("Operation log query page must report next page."));
    expect(logPage.items.first().action == QStringLiteral("Alarm.Acknowledged"),
           QStringLiteral("Operation log query page must sort newest first."));

    configurationRepository.saveRuntimeSettings({{QStringLiteral("UiRefreshIntervalMs"), QStringLiteral("1000")}});
    expect(configurationRepository.loadRuntimeSettings().value(QStringLiteral("UiRefreshIntervalMs")) == QStringLiteral("1000"),
           QStringLiteral("Configuration repository must round-trip runtime settings."));
}

void runCsvExportTests()
{
    QTemporaryDir directory;
    expect(directory.isValid(), QStringLiteral("Test must create a temporary export directory."));
    const auto csvPath = directory.filePath(QStringLiteral("export.csv"));
    QString error;
    expect(Monitor::Presentation::Export::CsvExportWriter::write(
               csvPath,
               {QStringLiteral("Name"), QStringLiteral("Text,Value")},
               {
                   {QStringLiteral("alpha"), QStringLiteral("Hello, \"Qt\"")},
                   {QStringLiteral("中文"), QStringLiteral("字段")}
               },
               &error),
           QStringLiteral("CsvExportWriter must write test CSV: %1").arg(error));

    QFile file(csvPath);
    expect(file.open(QIODevice::ReadOnly), QStringLiteral("Test must read exported CSV."));
    const auto bytes = file.readAll();
    expect(bytes.startsWith(QByteArray::fromHex("EFBBBF")), QStringLiteral("CSV must start with UTF-8 BOM for Excel."));
    const auto text = QString::fromUtf8(bytes.mid(3));
    expect(text.contains(QStringLiteral("\"Text,Value\"")), QStringLiteral("CSV headers must be escaped."));
    expect(text.contains(QStringLiteral("\"Hello, \"\"Qt\"\"\"")), QStringLiteral("CSV fields must escape quotes."));
    expect(text.contains(QStringLiteral("中文")), QStringLiteral("CSV must preserve UTF-8 text."));
}

void runUiSnapshotStartStopTests()
{
    Monitor::Application::Services::UiSnapshotProvider provider;

    const auto initial = provider.refresh(true);
    expect(!initial.shell.running, QStringLiteral("Initial UI snapshot must start stopped."));
    expect(initial.shell.databaseConnected, QStringLiteral("UI snapshot must reflect injected database state."));
    expect(initial.shell.lastFrameIndex >= 1, QStringLiteral("Initial UI snapshot must create a baseline frame."));
    expect(!initial.tags.currentValues.isEmpty(), QStringLiteral("Initial UI snapshot must contain tag values."));
    expect(initial.measurementMap.has_value(), QStringLiteral("Initial UI snapshot must contain a measurement map."));

    provider.setRunning(true);
    const auto running = provider.refresh(true);
    expect(running.shell.running, QStringLiteral("Started UI snapshot must report running."));
    expect(running.shell.syncState == QStringLiteral("Streaming"), QStringLiteral("Started UI snapshot must report Streaming sync state."));
    expect(running.shell.lastFrameIndex > initial.shell.lastFrameIndex, QStringLiteral("Running UI refresh must advance frame index."));

    provider.setRunning(false);
    const auto stopped = provider.refresh(true);
    expect(!stopped.shell.running, QStringLiteral("Stopped UI snapshot must report stopped."));
    expect(stopped.shell.syncState == QStringLiteral("Idle"), QStringLiteral("Stopped UI snapshot must report Idle sync state."));
    expect(stopped.shell.lastFrameIndex == running.shell.lastFrameIndex, QStringLiteral("Stopped UI refresh must retain last frame without acquisition."));
}

struct TestCase
{
    QString name;
    std::function<void()> run;
};

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    const QVector<TestCase> tests = {
        {QStringLiteral("LayerValidation"), runLayerValidationTests},
        {QStringLiteral("MatrixAndPipeline"), runMatrixAndPipelineTests},
        {QStringLiteral("AlarmLifecycle"), runAlarmLifecycleTests},
        {QStringLiteral("QueueAndWorker"), runQueueAndWorkerTests},
        {QStringLiteral("SqliteRepositories"), runSqliteRepositoryTests},
        {QStringLiteral("CsvExport"), runCsvExportTests},
        {QStringLiteral("UiSnapshotStartStop"), runUiSnapshotStartStopTests}
    };

    auto failed = 0;
    for (const auto &test : tests) {
        try {
            test.run();
            out << "[PASS] " << test.name << Qt::endl;
        } catch (const std::exception &exception) {
            ++failed;
            err << "[FAIL] " << test.name << ": " << QString::fromUtf8(exception.what()) << Qt::endl;
        } catch (...) {
            ++failed;
            err << "[FAIL] " << test.name << ": unknown exception" << Qt::endl;
        }
    }

    if (failed > 0) {
        err << failed << " test(s) failed." << Qt::endl;
        return 1;
    }

    out << tests.size() << " test(s) passed." << Qt::endl;
    return 0;
}
