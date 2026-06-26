#include "application/ApplicationLayer.h"
#include "application/abstractions/IRawFrameSource.h"
#include "application/events/EventBus.h"
#include "application/pipelines/DataCleanPipeline.h"
#include "application/queues/BlockingQueue.h"
#include "application/runtime/MonitoringRuntimeService.h"
#include "application/runtime/PersistenceRuntimeCoordinator.h"
#include "application/services/AlarmService.h"
#include "application/services/OperationLogService.h"
#include "application/services/QueryServices.h"
#include "application/services/RuntimeEventConsumers.h"
#include "application/services/TagDefinitionCatalog.h"
#include "application/services/UiSnapshotProvider.h"
#include "application/workers/BatchPersistWorker.h"
#include "bootstrap/RuntimeComposition.h"
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

bool containsLogAction(
    const QVector<Monitor::Domain::Logs::OperationLog> &logs,
    const QString &action)
{
    return std::any_of(logs.cbegin(), logs.cend(), [&action](const auto &log) {
        return log.action == action;
    });
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

void runRuntimeCompositionObjectGraphTests()
{
    QTemporaryDir directory;
    expect(directory.isValid(), QStringLiteral("Test must create a temporary composition database directory."));
    const auto databasePath = directory.filePath(QStringLiteral("composition.db"));

    {
        SqliteConnectionFactory factory(databasePath);
        factory.initialize();
        SQLiteConfigurationRepository configurationRepository(&factory);
        configurationRepository.saveRuntimeSettings({
            {Monitor::Application::Configuration::RuntimeSettingKeys::DataGenerateIntervalMs, QStringLiteral("750")},
            {Monitor::Application::Configuration::RuntimeSettingKeys::UiRefreshIntervalMs, QStringLiteral("250")}
        });

        const auto definitions = TagDefinitionCatalog::createDefaults();
        const auto vibrationDefinition = std::find_if(
            definitions.cbegin(),
            definitions.cend(),
            [](const auto &definition) {
                return definition.tagId == QStringLiteral("MEAS.VIBRATION.CH01");
            });
        expect(vibrationDefinition != definitions.cend(), QStringLiteral("Default catalog must contain vibration tag."));
        auto vibration = Monitor::Application::Configuration::TagRuntimeConfiguration::fromDefinition(*vibrationDefinition);
        vibration.alarmHigh = 3.5;
        configurationRepository.saveTagConfigurations({vibration});
    }

    auto dependencies = Monitor::Bootstrap::RuntimeCompositionDependencies::createDefault();
    dependencies.databasePath = databasePath;

    Monitor::Bootstrap::RuntimeComposition composition(dependencies);
    QStringList errors;
    const auto initialized = composition.initialize(&errors);
    expect(
        initialized,
        QStringLiteral("RuntimeComposition must initialize full object graph. ErrorCount=%1 Errors=%2")
            .arg(errors.size())
            .arg(errors.join(QStringLiteral("; "))));

    expect(composition.sqliteConnectionFactory(), QStringLiteral("Composition must expose SQLite connection factory."));
    expect(composition.historyRepository(), QStringLiteral("Composition must expose history repository."));
    expect(composition.alarmRepository(), QStringLiteral("Composition must expose alarm repository."));
    expect(composition.operationLogRepository(), QStringLiteral("Composition must expose operation log repository."));
    expect(composition.configurationRepository(), QStringLiteral("Composition must expose configuration repository."));
    expect(composition.eventBus(), QStringLiteral("Composition must expose EventBus."));
    expect(!composition.tagDefinitions().isEmpty(), QStringLiteral("Composition must hold default tag definitions."));
    expect(composition.tagSourceMappings().size() == composition.tagDefinitions().size(), QStringLiteral("Composition must hold tag source mappings."));
    expect(composition.tagRuntimeConfigurations().size() == composition.tagDefinitions().size(), QStringLiteral("Composition must merge tag runtime configurations."));
    expect(composition.runtimeOptions().dataGenerateIntervalMs == 750, QStringLiteral("Composition must load runtime settings from SQLite."));
    expect(composition.runtimeOptions().uiRefreshIntervalMs == 250, QStringLiteral("Composition must load UI refresh setting from SQLite."));

    const auto configIt = std::find_if(
        composition.tagRuntimeConfigurations().cbegin(),
        composition.tagRuntimeConfigurations().cend(),
        [](const auto &configuration) {
            return configuration.tagId == QStringLiteral("MEAS.VIBRATION.CH01");
        });
    expect(configIt != composition.tagRuntimeConfigurations().cend(), QStringLiteral("Composition must include vibration tag configuration."));
    expect(configIt->alarmHigh.has_value() && configIt->alarmHigh.value() == 3.5, QStringLiteral("Composition must load tag runtime configuration overrides."));

    expect(composition.runtimeOptionsStore(), QStringLiteral("Composition must expose RuntimeOptionsStore."));
    expect(composition.tagRuntimeConfigurationStore(), QStringLiteral("Composition must expose TagRuntimeConfigurationStore."));
    expect(composition.dataCleanPipeline(), QStringLiteral("Composition must expose DataCleanPipeline."));
    expect(composition.alarmService(), QStringLiteral("Composition must expose AlarmService."));
    expect(composition.tagService(), QStringLiteral("Composition must expose TagService."));
    expect(composition.dashboardService(), QStringLiteral("Composition must expose DashboardService."));
    expect(composition.chartDataService(), QStringLiteral("Composition must expose ChartDataService."));
    expect(composition.measurementMapService(), QStringLiteral("Composition must expose MeasurementMapService."));
    expect(composition.operationLogService(), QStringLiteral("Composition must expose OperationLogService."));
    expect(composition.historyQueryService(), QStringLiteral("Composition must expose HistoryQueryService."));
    expect(composition.alarmQueryService(), QStringLiteral("Composition must expose AlarmQueryService."));
    expect(composition.operationLogQueryService(), QStringLiteral("Composition must expose OperationLogQueryService."));
    expect(composition.runtimeCommandFacade(), QStringLiteral("Composition must expose RuntimeCommandFacade."));
    expect(composition.runtimeUiSnapshotProvider(), QStringLiteral("Composition must expose RuntimeUiSnapshotProvider."));
    expect(composition.dataSourceHealthMonitor(), QStringLiteral("Composition must expose DataSourceHealthMonitor."));
    expect(composition.simulatorDataSource(), QStringLiteral("Composition must expose SimulatorDataSource."));
    expect(composition.monitoringRuntimeService(), QStringLiteral("Composition must expose MonitoringRuntimeService."));
    expect(composition.historySampleQueue(), QStringLiteral("Composition must expose history queue."));
    expect(composition.alarmEventQueue(), QStringLiteral("Composition must expose alarm queue."));
    expect(composition.operationLogQueue(), QStringLiteral("Composition must expose operation log queue."));
    expect(composition.historyPersistWorker() && composition.historyPersistWorker()->name() == QStringLiteral("History"), QStringLiteral("Composition must expose history worker."));
    expect(composition.alarmPersistWorker() && composition.alarmPersistWorker()->name() == QStringLiteral("Alarm"), QStringLiteral("Composition must expose alarm worker."));
    expect(composition.operationLogPersistWorker() && composition.operationLogPersistWorker()->name() == QStringLiteral("OperationLog"), QStringLiteral("Composition must expose operation log worker."));
    expect(composition.persistenceRuntimeCoordinator(), QStringLiteral("Composition must expose PersistenceRuntimeCoordinator."));
    expect(composition.runtimeLifecycleCoordinator(), QStringLiteral("Composition must expose RuntimeLifecycleCoordinator."));
    expect(composition.acquisitionRuntimeController(), QStringLiteral("Composition must expose AcquisitionRuntimeController."));
}

void runEventBusHandlersDriveRuntimeConsumersTests()
{
    QTemporaryDir directory;
    expect(directory.isValid(), QStringLiteral("Test must create a temporary composition database directory."));

    auto dependencies = Monitor::Bootstrap::RuntimeCompositionDependencies::createDefault();
    dependencies.databasePath = directory.filePath(QStringLiteral("eventbus-consumers.db"));

    Monitor::Bootstrap::RuntimeComposition composition(dependencies);
    QStringList initializeErrors;
    expect(
        composition.initialize(&initializeErrors),
        QStringLiteral("RuntimeComposition must initialize event handlers: %1")
            .arg(initializeErrors.join(QStringLiteral("; "))));
    expect(composition.tagCacheConsumer(), QStringLiteral("Composition must own TagCacheConsumer."));
    expect(composition.measurementMapFrameConsumer(), QStringLiteral("Composition must own MeasurementMapFrameConsumer."));
    expect(composition.historyRuntimeStateConsumer(), QStringLiteral("Composition must own HistoryRuntimeStateConsumer."));
    expect(composition.alarmEventConsumer(), QStringLiteral("Composition must own AlarmEventConsumer."));
    expect(composition.alarmOperationLogConsumer(), QStringLiteral("Composition must own AlarmOperationLogConsumer."));
    expect(composition.dataSourceHealthOperationLogConsumer(), QStringLiteral("Composition must own DataSourceHealthOperationLogConsumer."));

    auto frame = createFrame(42, utcTime(20'000));
    for (auto &channel : frame.channelValues) {
        if (channel.channelId == QStringLiteral("MEAS.VIBRATION.CH01") ||
            channel.channelId == QStringLiteral("VIBRATION_CH01")) {
            channel.value = 3.2;
        }
    }

    QStringList processErrors;
    expect(
        composition.monitoringRuntimeService()->processFrame(frame, &processErrors),
        QStringLiteral("Runtime processFrame must publish through bound handlers: %1")
            .arg(processErrors.join(QStringLiteral("; "))));

    expect(!composition.tagService()->snapshot().currentValues.isEmpty(), QStringLiteral("TagCacheConsumer must update TagService cache."));
    expect(composition.measurementMapService()->latestSnapshot().has_value(), QStringLiteral("MeasurementMapFrameConsumer must update latest matrix snapshot."));
    expect(composition.historySampleQueue()->size() > 0, QStringLiteral("HistoryRuntimeStateConsumer must enqueue history samples."));
    expect(composition.alarmEventQueue()->size() > 0, QStringLiteral("AlarmEventConsumer must enqueue raised alarm events."));
    expect(composition.operationLogQueue()->size() > 0, QStringLiteral("AlarmOperationLogConsumer must enqueue operation logs."));

    const auto currentAlarms = composition.alarmService()->currentAlarms();
    expect(!currentAlarms.isEmpty(), QStringLiteral("AlarmService must keep active alarms after handler-driven frame processing."));
    const auto alarmQueueSizeBeforeAcknowledge = composition.alarmEventQueue()->size();
    const auto logQueueSizeBeforeAcknowledge = composition.operationLogQueue()->size();

    AlarmEvent acknowledged;
    QStringList acknowledgeErrors;
    expect(
        composition.monitoringRuntimeService()->acknowledgeAlarm(
            currentAlarms.first().alarmId,
            utcTime(20'500),
            &acknowledged,
            &acknowledgeErrors),
        QStringLiteral("Acknowledge must publish through bound alarm handlers: %1")
            .arg(acknowledgeErrors.join(QStringLiteral("; "))));
    expect(acknowledged.state == AlarmState::Acknowledged, QStringLiteral("Acknowledged alarm must be returned from runtime service."));
    expect(composition.alarmEventQueue()->size() > alarmQueueSizeBeforeAcknowledge, QStringLiteral("AlarmEventConsumer must enqueue acknowledged alarm event."));
    expect(composition.operationLogQueue()->size() > logQueueSizeBeforeAcknowledge, QStringLiteral("AlarmOperationLogConsumer must enqueue acknowledged operation log."));
}

void runApplicationRuntimeHostLifecycleTests()
{
    QTemporaryDir directory;
    expect(directory.isValid(), QStringLiteral("Test must create a temporary host database directory."));

    auto dependencies = Monitor::Bootstrap::RuntimeCompositionDependencies::createDefault();
    dependencies.databasePath = directory.filePath(QStringLiteral("application-host.db"));

    Monitor::Bootstrap::RuntimeComposition composition(dependencies);
    QStringList initializeErrors;
    expect(
        composition.initialize(&initializeErrors),
        QStringLiteral("RuntimeComposition must initialize host dependencies: %1")
            .arg(initializeErrors.join(QStringLiteral("; "))));
    expect(composition.applicationRuntimeHost(), QStringLiteral("Composition must expose ApplicationRuntimeHost."));

    QStringList startErrors;
    expect(
        composition.applicationRuntimeHost()->start(&startErrors),
        QStringLiteral("ApplicationRuntimeHost must start persistence runtime: %1")
            .arg(startErrors.join(QStringLiteral("; "))));
    expect(composition.applicationRuntimeHost()->isStarted(), QStringLiteral("ApplicationRuntimeHost must report started."));
    expect(composition.persistenceRuntimeCoordinator()->isRunning(), QStringLiteral("Persistence runtime must be running after host start."));

    auto frame = createFrame(84, utcTime(40'000));
    for (auto &channel : frame.channelValues) {
        if (channel.channelId == QStringLiteral("VIBRATION_CH01")) {
            channel.value = 3.4;
        }
    }

    QStringList processErrors;
    expect(
        composition.monitoringRuntimeService()->processFrame(frame, &processErrors),
        QStringLiteral("Runtime frame processing must succeed while host persistence is running: %1")
            .arg(processErrors.join(QStringLiteral("; "))));

    QStringList stopErrors;
    expect(
        composition.applicationRuntimeHost()->stop(&stopErrors),
        QStringLiteral("ApplicationRuntimeHost must stop and flush cleanly: %1")
            .arg(stopErrors.join(QStringLiteral("; "))));
    expect(!composition.applicationRuntimeHost()->isStarted(), QStringLiteral("ApplicationRuntimeHost must report stopped."));
    expect(!composition.persistenceRuntimeCoordinator()->isRunning(), QStringLiteral("Persistence runtime must be stopped after host shutdown."));
    expect(composition.historySampleQueue()->size() == 0, QStringLiteral("Host shutdown must drain history queue."));
    expect(composition.alarmEventQueue()->size() == 0, QStringLiteral("Host shutdown must drain alarm queue."));
    expect(composition.operationLogQueue()->size() == 0, QStringLiteral("Host shutdown must drain operation log queue."));

    const auto history = composition.historyRepository()->query(
        QStringLiteral("MEAS.VIBRATION.CH01"),
        frame.timestampUtc.addMSecs(-1),
        frame.timestampUtc.addMSecs(1));
    expect(!history.isEmpty(), QStringLiteral("Host shutdown must persist queued history samples."));
    expect(!composition.alarmRepository()->queryLatest(10).isEmpty(), QStringLiteral("Host shutdown must persist queued alarm events."));
    const auto logs = composition.operationLogRepository()->queryLatest(50);
    expect(containsLogAction(logs, QStringLiteral("Alarm.Raised")), QStringLiteral("Host shutdown must persist alarm operation logs."));
    expect(containsLogAction(logs, QStringLiteral("ApplicationRuntimeHost.Stopping")), QStringLiteral("Host shutdown must persist application stop log."));
    expect(containsLogAction(logs, QStringLiteral("History.RetentionCleanup")), QStringLiteral("Host startup must execute history retention cleanup."));
}

void runPageQueryServicesReadSqliteTests()
{
    QTemporaryDir directory;
    expect(directory.isValid(), QStringLiteral("Test must create a temporary query service database directory."));

    auto dependencies = Monitor::Bootstrap::RuntimeCompositionDependencies::createDefault();
    dependencies.databasePath = directory.filePath(QStringLiteral("page-query-services.db"));

    Monitor::Bootstrap::RuntimeComposition composition(dependencies);
    QStringList initializeErrors;
    expect(
        composition.initialize(&initializeErrors),
        QStringLiteral("RuntimeComposition must initialize query services: %1")
            .arg(initializeErrors.join(QStringLiteral("; "))));

    QStringList startErrors;
    expect(
        composition.applicationRuntimeHost()->start(&startErrors),
        QStringLiteral("Host must start before query service persistence check: %1")
            .arg(startErrors.join(QStringLiteral("; "))));

    auto frame = createFrame(168, utcTime(80'000));
    for (auto &channel : frame.channelValues) {
        if (channel.channelId == QStringLiteral("VIBRATION_CH01")) {
            channel.value = 3.6;
        }
    }

    QStringList processErrors;
    expect(
        composition.monitoringRuntimeService()->processFrame(frame, &processErrors),
        QStringLiteral("Runtime frame processing must enqueue data for query services: %1")
            .arg(processErrors.join(QStringLiteral("; "))));

    QStringList stopErrors;
    expect(
        composition.applicationRuntimeHost()->stop(&stopErrors),
        QStringLiteral("Host shutdown must flush query service persistence inputs: %1")
            .arg(stopErrors.join(QStringLiteral("; "))));

    const auto historyPage = composition.historyQueryService()->query(
        Monitor::Application::Services::HistoryQueryRequest{
            QStringLiteral("MEAS.VIBRATION.CH01"),
            frame.timestampUtc.addMSecs(-1),
            frame.timestampUtc.addMSecs(1),
            1,
            10,
            true
        });
    expect(!historyPage.items.isEmpty(), QStringLiteral("HistoryQueryService must read persisted SQLite history samples."));
    expect(historyPage.totalCount >= historyPage.items.size(), QStringLiteral("HistoryQueryService must report total history sample count."));

    const auto alarmPage = composition.alarmQueryService()->query(
        Monitor::Application::Services::AlarmHistoryQueryRequest{
            frame.timestampUtc.addMSecs(-1),
            frame.timestampUtc.addMSecs(1),
            QStringLiteral("MEAS.VIBRATION.CH01"),
            std::nullopt,
            std::nullopt,
            1,
            10,
            false
        });
    expect(!alarmPage.items.isEmpty(), QStringLiteral("AlarmQueryService must read persisted SQLite alarm events."));
    expect(alarmPage.items.first().tagId == QStringLiteral("MEAS.VIBRATION.CH01"), QStringLiteral("AlarmQueryService must honor tag filters."));

    const auto nowUtc = QDateTime::currentDateTimeUtc();
    const auto logPage = composition.operationLogQueryService()->query(
        Monitor::Application::Services::OperationLogQueryRequest{
            nowUtc.addDays(-1),
            nowUtc.addDays(1),
            std::nullopt,
            QStringLiteral("Raised"),
            1,
            20
        });
    expect(!logPage.items.isEmpty(), QStringLiteral("OperationLogQueryService must read persisted SQLite operation logs."));
    expect(containsLogAction(logPage.items, QStringLiteral("Alarm.Raised")), QStringLiteral("OperationLogQueryService must support action text filtering."));
}

void runRuntimeUiSnapshotProviderReadsRuntimeStateTests()
{
    QTemporaryDir directory;
    expect(directory.isValid(), QStringLiteral("Test must create a temporary snapshot database directory."));

    auto dependencies = Monitor::Bootstrap::RuntimeCompositionDependencies::createDefault();
    dependencies.databasePath = directory.filePath(QStringLiteral("runtime-snapshot.db"));

    Monitor::Bootstrap::RuntimeComposition composition(dependencies);
    QStringList initializeErrors;
    expect(
        composition.initialize(&initializeErrors),
        QStringLiteral("RuntimeComposition must initialize snapshot provider dependencies: %1")
            .arg(initializeErrors.join(QStringLiteral("; "))));

    const auto initial = composition.runtimeUiSnapshotProvider()->refresh();
    expect(!initial.shell.running, QStringLiteral("Initial runtime snapshot must report stopped acquisition."));
    expect(initial.shell.databaseConnected, QStringLiteral("Initial runtime snapshot must report initialized database."));
    expect(initial.shell.lastFrameIndex == 0, QStringLiteral("Initial runtime snapshot must not synthesize a frame index."));
    expect(initial.tags.currentValues.isEmpty(), QStringLiteral("Initial runtime snapshot must not synthesize tag values."));
    expect(!initial.measurementMap.has_value(), QStringLiteral("Initial runtime snapshot must not synthesize a measurement map."));
    expect(initial.tagDefinitions.size() == composition.tagDefinitions().size(), QStringLiteral("Runtime snapshot must expose tag definitions."));
    expect(initial.tagConfigurations.size() == composition.tagRuntimeConfigurations().size(), QStringLiteral("Runtime snapshot must expose tag runtime configurations."));

    auto frame = createFrame(126, utcTime(60'000));
    for (auto &channel : frame.channelValues) {
        if (channel.channelId == QStringLiteral("VIBRATION_CH01")) {
            channel.value = 3.6;
        }
    }

    QStringList processErrors;
    expect(
        composition.monitoringRuntimeService()->processFrame(frame, &processErrors),
        QStringLiteral("Runtime frame processing must update snapshot source caches: %1")
            .arg(processErrors.join(QStringLiteral("; "))));

    const auto updated = composition.runtimeUiSnapshotProvider()->refresh();
    expect(!updated.shell.running, QStringLiteral("Direct frame processing must not mark acquisition lifecycle running."));
    expect(updated.shell.dataSourceConnected, QStringLiteral("Runtime snapshot must reflect online data source health after a real frame."));
    expect(updated.shell.lastFrameIndex == static_cast<quint64>(frame.sequenceNo), QStringLiteral("Runtime snapshot must report the real latest frame index."));
    expect(updated.shell.matrixFrameIndex == frame.sequenceNo, QStringLiteral("Runtime snapshot must report the real latest matrix frame index."));
    expect(!updated.tags.currentValues.isEmpty(), QStringLiteral("Runtime snapshot must read tag cache values."));
    expect(updated.dashboard.sequenceNo == frame.sequenceNo, QStringLiteral("Dashboard snapshot must be built from runtime tag cache."));
    expect(!updated.currentAlarms.isEmpty(), QStringLiteral("Runtime snapshot must read current alarms."));
    expect(!updated.alarmHistory.isEmpty(), QStringLiteral("Runtime snapshot must read recent alarm events."));
    expect(updated.measurementMap.has_value(), QStringLiteral("Runtime snapshot must read measurement map cache."));
    expect(updated.measurementMap->sequenceNo == frame.sequenceNo, QStringLiteral("Measurement map snapshot must use real frame sequence."));

    const auto repeated = composition.runtimeUiSnapshotProvider()->refresh();
    expect(repeated.shell.lastFrameIndex == updated.shell.lastFrameIndex, QStringLiteral("Stopped runtime snapshot refresh must not generate new frames."));
    expect(repeated.tags.currentValues.size() == updated.tags.currentValues.size(), QStringLiteral("Stopped runtime snapshot refresh must retain runtime cache values."));
}

void runRuntimeCommandFacadeControlsRuntimeTests()
{
    QTemporaryDir directory;
    expect(directory.isValid(), QStringLiteral("Test must create a temporary command facade database directory."));

    auto dependencies = Monitor::Bootstrap::RuntimeCompositionDependencies::createDefault();
    dependencies.databasePath = directory.filePath(QStringLiteral("runtime-command-facade.db"));

    Monitor::Bootstrap::RuntimeComposition composition(dependencies);
    QStringList initializeErrors;
    expect(
        composition.initialize(&initializeErrors),
        QStringLiteral("RuntimeComposition must initialize command facade dependencies: %1")
            .arg(initializeErrors.join(QStringLiteral("; "))));

    QStringList hostErrors;
    expect(
        composition.applicationRuntimeHost()->start(&hostErrors),
        QStringLiteral("Host must start persistence before command facade start: %1")
            .arg(hostErrors.join(QStringLiteral("; "))));

    QStringList startErrors;
    expect(
        composition.runtimeCommandFacade()->start(&startErrors),
        QStringLiteral("RuntimeCommandFacade must start acquisition runtime: %1")
            .arg(startErrors.join(QStringLiteral("; "))));
    for (auto attempt = 0; attempt < 20 && !composition.runtimeLifecycleCoordinator()->isActive(); ++attempt) {
        QThread::msleep(25);
    }
    expect(composition.runtimeLifecycleCoordinator()->isActive(), QStringLiteral("Runtime lifecycle must become active after facade start."));

    QStringList stopErrors;
    expect(
        composition.runtimeCommandFacade()->stop(&stopErrors),
        QStringLiteral("RuntimeCommandFacade must stop acquisition runtime: %1")
            .arg(stopErrors.join(QStringLiteral("; "))));
    expect(!composition.runtimeLifecycleCoordinator()->isActive(), QStringLiteral("Runtime lifecycle must become inactive after facade stop."));

    auto options = composition.runtimeOptionsStore()->snapshot();
    options.uiRefreshIntervalMs = 777;
    QStringList saveErrors;
    expect(
        composition.runtimeCommandFacade()->saveRuntimeOptions(options, &saveErrors),
        QStringLiteral("RuntimeCommandFacade must save runtime options in memory: %1")
            .arg(saveErrors.join(QStringLiteral("; "))));
    expect(
        composition.runtimeUiSnapshotProvider()->refresh().runtimeOptions.uiRefreshIntervalMs == 777,
        QStringLiteral("Runtime snapshot provider must read runtime options updated through facade."));

    QStringList shutdownErrors;
    expect(
        composition.applicationRuntimeHost()->stop(&shutdownErrors),
        QStringLiteral("Host shutdown must flush command facade logs: %1")
            .arg(shutdownErrors.join(QStringLiteral("; "))));
}

void runSettingsSaveReloadsFromSqliteTests()
{
    QTemporaryDir directory;
    expect(directory.isValid(), QStringLiteral("Test must create a temporary settings database directory."));

    const auto databasePath = directory.filePath(QStringLiteral("settings-save-reload.db"));
    auto dependencies = Monitor::Bootstrap::RuntimeCompositionDependencies::createDefault();
    dependencies.databasePath = databasePath;

    Monitor::Bootstrap::RuntimeComposition composition(dependencies);
    QStringList initializeErrors;
    expect(
        composition.initialize(&initializeErrors),
        QStringLiteral("RuntimeComposition must initialize settings save dependencies: %1")
            .arg(initializeErrors.join(QStringLiteral("; "))));

    auto options = composition.runtimeOptionsStore()->snapshot();
    options.uiRefreshIntervalMs = 333;
    options.dataGenerateIntervalMs = 650;
    options.historyRetentionDays = 11;
    QStringList runtimeSaveErrors;
    expect(
        composition.runtimeCommandFacade()->saveRuntimeOptions(options, &runtimeSaveErrors),
        QStringLiteral("RuntimeCommandFacade must persist runtime options: %1")
            .arg(runtimeSaveErrors.join(QStringLiteral("; "))));
    expect(
        composition.runtimeUiSnapshotProvider()->refresh().runtimeOptions.uiRefreshIntervalMs == 333,
        QStringLiteral("Runtime options save must update RuntimeOptionsStore immediately."));
    const auto savedSettings = composition.configurationRepository()->loadRuntimeSettings();
    expect(
        savedSettings.value(Monitor::Application::Configuration::RuntimeSettingKeys::UiRefreshIntervalMs) == QStringLiteral("333"),
        QStringLiteral("Runtime options save must persist UI refresh interval to SQLite."));
    expect(
        savedSettings.value(Monitor::Application::Configuration::RuntimeSettingKeys::DataGenerateIntervalMs) == QStringLiteral("650"),
        QStringLiteral("Runtime options save must persist acquisition interval to SQLite."));

    auto configurations = composition.tagRuntimeConfigurations();
    auto vibrationIt = std::find_if(configurations.begin(), configurations.end(), [](const auto &configuration) {
        return configuration.tagId == QStringLiteral("MEAS.VIBRATION.CH01");
    });
    expect(vibrationIt != configurations.end(), QStringLiteral("Default configurations must include vibration tag."));
    vibrationIt->warningHigh = 3.0;
    vibrationIt->alarmHigh = 4.0;
    vibrationIt->isHistorized = false;
    vibrationIt->historyIntervalMs = 60'000;

    QStringList tagSaveErrors;
    expect(
        composition.runtimeCommandFacade()->saveTagConfigurations(configurations, &tagSaveErrors),
        QStringLiteral("RuntimeCommandFacade must persist tag configurations: %1")
            .arg(tagSaveErrors.join(QStringLiteral("; "))));

    QStringList hostErrors;
    expect(
        composition.applicationRuntimeHost()->start(&hostErrors),
        QStringLiteral("Host must start to verify saved settings runtime synchronization: %1")
            .arg(hostErrors.join(QStringLiteral("; "))));

    auto frame = createFrame(210, utcTime(90'000));
    for (auto &channel : frame.channelValues) {
        if (channel.channelId == QStringLiteral("VIBRATION_CH01")) {
            channel.value = 3.6;
        }
    }

    QStringList processErrors;
    expect(
        composition.monitoringRuntimeService()->processFrame(frame, &processErrors),
        QStringLiteral("Runtime frame processing must use synchronized tag settings: %1")
            .arg(processErrors.join(QStringLiteral("; "))));
    const auto activeAlarm = findAlarm(composition.alarmService()->currentAlarms(), QStringLiteral("MEAS.VIBRATION.CH01"));
    expect(activeAlarm != nullptr, QStringLiteral("Saved alarm thresholds must still evaluate vibration alarm state."));
    expect(activeAlarm->level == Monitor::Domain::Alarms::AlarmLevel::Warning,
           QStringLiteral("Saved alarm thresholds must downgrade 3.6 vibration from alarm to warning."));

    QStringList stopErrors;
    expect(
        composition.applicationRuntimeHost()->stop(&stopErrors),
        QStringLiteral("Host shutdown must flush after saved settings verification: %1")
            .arg(stopErrors.join(QStringLiteral("; "))));

    const auto historyPage = composition.historyQueryService()->query(
        Monitor::Application::Services::HistoryQueryRequest{
            QStringLiteral("MEAS.VIBRATION.CH01"),
            frame.timestampUtc.addMSecs(-1),
            frame.timestampUtc.addMSecs(1),
            1,
            10,
            true
        });
    expect(historyPage.totalCount == 0, QStringLiteral("Saved historized=false setting must stop new vibration history samples."));

    Monitor::Bootstrap::RuntimeComposition reloaded(dependencies);
    QStringList reloadErrors;
    expect(
        reloaded.initialize(&reloadErrors),
        QStringLiteral("RuntimeComposition must reload saved settings from SQLite: %1")
            .arg(reloadErrors.join(QStringLiteral("; "))));
    expect(
        reloaded.runtimeOptions().uiRefreshIntervalMs == 333,
        QStringLiteral("Reloaded composition must restore saved UI refresh interval."));
    expect(
        reloaded.runtimeOptions().dataGenerateIntervalMs == 650,
        QStringLiteral("Reloaded composition must restore saved acquisition interval."));
    const auto reloadedVibrationIt = std::find_if(
        reloaded.tagRuntimeConfigurations().cbegin(),
        reloaded.tagRuntimeConfigurations().cend(),
        [](const auto &configuration) {
            return configuration.tagId == QStringLiteral("MEAS.VIBRATION.CH01");
        });
    expect(reloadedVibrationIt != reloaded.tagRuntimeConfigurations().cend(), QStringLiteral("Reloaded configurations must include vibration tag."));
    expect(!reloadedVibrationIt->isHistorized, QStringLiteral("Reloaded tag configuration must preserve historized=false."));
    expect(reloadedVibrationIt->alarmHigh.has_value() && reloadedVibrationIt->alarmHigh.value() == 4.0,
           QStringLiteral("Reloaded tag configuration must preserve saved alarm threshold."));
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
        {QStringLiteral("UiSnapshotStartStop"), runUiSnapshotStartStopTests},
        {QStringLiteral("RuntimeCompositionObjectGraph"), runRuntimeCompositionObjectGraphTests},
        {QStringLiteral("EventBusHandlersDriveRuntimeConsumers"), runEventBusHandlersDriveRuntimeConsumersTests},
        {QStringLiteral("ApplicationRuntimeHostLifecycle"), runApplicationRuntimeHostLifecycleTests},
        {QStringLiteral("PageQueryServicesReadSqlite"), runPageQueryServicesReadSqliteTests},
        {QStringLiteral("RuntimeUiSnapshotProviderReadsRuntimeState"), runRuntimeUiSnapshotProviderReadsRuntimeStateTests},
        {QStringLiteral("RuntimeCommandFacadeControlsRuntime"), runRuntimeCommandFacadeControlsRuntimeTests},
        {QStringLiteral("SettingsSaveReloadsFromSqlite"), runSettingsSaveReloadsFromSqliteTests}
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
