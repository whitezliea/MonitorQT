#include "UiSnapshotProvider.h"

#include "application/services/TagDefinitionCatalog.h"
#include "domain/common/DomainCommon.h"
#include "domain/devices/DeviceModels.h"

#include <QtMath>

#include <algorithm>

namespace Monitor::Application::Services {
namespace {

QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> defaultConfigurations(
    const QVector<Monitor::Domain::Tags::TagDefinition> &definitions)
{
    QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> configurations;
    configurations.reserve(definitions.size());
    for (const auto &definition : definitions) {
        configurations.append(Monitor::Application::Configuration::TagRuntimeConfiguration::fromDefinition(definition));
    }
    return configurations;
}

QString sourceFor(const Monitor::Domain::Measurements::RawMeasurementFrame &frame)
{
    return frame.frameId.toString(QUuid::WithoutBraces);
}

QString formatDouble(double value, int decimals = 2)
{
    return QString::number(value, 'f', decimals);
}

Monitor::Domain::Tags::TagValue historySampleFromState(
    const Monitor::Domain::Tags::TagRuntimeState &state)
{
    return {
        state.tagId,
        state.numericValue.value_or(state.boolValue.value_or(false) ? 1.0 : 0.0),
        state.timestampUtc,
        state.quality,
        state.alarmState,
        state.sourceFrameId.toString(QUuid::WithoutBraces),
        state.sequenceNo
    };
}

} // namespace

UiSnapshotProvider::UiSnapshotProvider()
    : m_definitions(TagDefinitionCatalog::createDefaults()),
      m_configurations(defaultConfigurations(m_definitions)),
      m_pipeline(m_definitions, TagDefinitionCatalog::createSourceMappings()),
      m_tagService(m_options.trendBufferCapacity()),
      m_alarmService(m_definitions, m_configurations),
      m_dashboardService(&m_tagService, &m_alarmService),
      m_chartDataService(&m_tagService)
{
    appendLog(
        Monitor::Domain::Logs::OperationLogLevel::Info,
        QStringLiteral("System"),
        QStringLiteral("UiSnapshotProvider.Initialized"),
        QStringLiteral("Qt presentation snapshot provider initialized."));
}

void UiSnapshotProvider::setRunning(bool running)
{
    if (m_running == running) {
        return;
    }

    m_running = running;
    appendLog(
        Monitor::Domain::Logs::OperationLogLevel::Info,
        QStringLiteral("System"),
        running ? QStringLiteral("Runtime.Start") : QStringLiteral("Runtime.Stop"),
        running ? QStringLiteral("Monitoring runtime started from UI.") : QStringLiteral("Monitoring runtime stopped from UI."));
}

bool UiSnapshotProvider::isRunning() const
{
    return m_running;
}

bool UiSnapshotProvider::acknowledgeAlarm(const QUuid &alarmId)
{
    Monitor::Domain::Alarms::AlarmEvent acknowledged;
    const auto success = m_alarmService.acknowledge(alarmId, QDateTime::currentDateTimeUtc(), &acknowledged);
    if (success) {
        appendLog(
            Monitor::Domain::Logs::OperationLogLevel::Info,
            QStringLiteral("Alarm"),
            QStringLiteral("Alarm.Acknowledged"),
            QStringLiteral("Alarm acknowledged from UI."),
            QStringLiteral("AlarmId=%1; TagId=%2").arg(alarmId.toString(QUuid::WithoutBraces), acknowledged.tagId));
    }
    return success;
}

void UiSnapshotProvider::saveRuntimeOptions(
    const Monitor::Application::Configuration::MonitorRuntimeOptions &options)
{
    m_options = options;
    appendLog(
        Monitor::Domain::Logs::OperationLogLevel::Info,
        QStringLiteral("Settings"),
        QStringLiteral("RuntimeOptions.Save"),
        QStringLiteral("Runtime options saved from UI."),
        QStringLiteral("UiRefreshIntervalMs=%1; HistoryRetentionDays=%2")
            .arg(m_options.uiRefreshIntervalMs)
            .arg(m_options.historyRetentionDays));
}

void UiSnapshotProvider::saveTagConfigurations(
    const QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> &configurations)
{
    if (configurations.isEmpty()) {
        return;
    }

    m_configurations = configurations;
    m_alarmService.replaceConfigurations(m_configurations);
    appendLog(
        Monitor::Domain::Logs::OperationLogLevel::Info,
        QStringLiteral("Settings"),
        QStringLiteral("TagConfigurations.Save"),
        QStringLiteral("Tag runtime configurations saved from UI."),
        QStringLiteral("Count=%1").arg(m_configurations.size()));
}

Monitor::Application::Dtos::UiSnapshot UiSnapshotProvider::refresh(bool databaseConnected)
{
    const auto nowUtc = QDateTime::currentDateTimeUtc();
    if (m_running || m_tagService.snapshot().currentValues.isEmpty()) {
        processFrame(nextFrame(nowUtc));
    }

    Monitor::Application::Dtos::UiSnapshot snapshot;
    snapshot.shell.running = m_running;
    snapshot.shell.dataSourceConnected = m_running;
    snapshot.shell.databaseConnected = databaseConnected;
    snapshot.shell.lastFrameIndex = m_sequenceNo;
    snapshot.shell.matrixFrameIndex = m_sequenceNo;
    snapshot.shell.syncState = m_running ? QStringLiteral("Streaming") : QStringLiteral("Idle");
    snapshot.shell.capturedAtUtc = nowUtc;
    snapshot.tags = m_tagService.snapshot();
    snapshot.dashboard = m_dashboardService.buildSnapshot(
        snapshot.tags,
        m_alarmService.currentAlarms(),
        nowUtc);
    snapshot.currentAlarms = m_alarmService.currentAlarms();
    snapshot.alarmHistory = m_alarmService.alarmEvents();
    snapshot.historySamples = m_historySamples;
    snapshot.operationLogs = m_operationLogs;
    snapshot.measurementMap = m_measurementMapService.latestSnapshot();
    snapshot.runtimeOptions = m_options;
    snapshot.tagConfigurations = m_configurations;
    snapshot.tagDefinitions = m_definitions;
    return snapshot;
}

Monitor::Domain::Measurements::RawMeasurementFrame UiSnapshotProvider::nextFrame(const QDateTime &timestampUtc)
{
    using Monitor::Domain::Measurements::ChannelValue;
    using Monitor::Domain::Measurements::MatrixFrame;
    using Monitor::Domain::Measurements::RawMeasurementFrame;
    using Monitor::Domain::Tags::TagQuality;

    ++m_sequenceNo;
    const auto phase = static_cast<double>(m_sequenceNo);
    RawMeasurementFrame frame;
    frame.frameId = QUuid::createUuid();
    frame.deviceId = TagDefinitionCatalog::defaultDeviceId();
    frame.sequenceNo = static_cast<qint64>(m_sequenceNo);
    frame.timestampUtc = Monitor::Domain::Common::UtcDateTime::require(timestampUtc, QStringLiteral("timestampUtc"));
    frame.deviceStatus = Monitor::Domain::Devices::DeviceStatus::Running;
    frame.quality = TagQuality::Good;

    const auto vibrationSpike = (m_sequenceNo % 18) >= 6 && (m_sequenceNo % 18) <= 10;
    const auto voltageDrop = (m_sequenceNo % 28) >= 20 && (m_sequenceNo % 28) <= 23;
    frame.channelValues = {
        ChannelValue{QStringLiteral("TEMP_CH01"), 28.0 + qSin(phase / 5.0) * 4.0, QStringLiteral("C"), TagQuality::Good, 0},
        ChannelValue{QStringLiteral("PRESSURE_CH01"), 101.0 + qCos(phase / 7.0) * 3.0, QStringLiteral("kPa"), TagQuality::Good, 0},
        ChannelValue{QStringLiteral("LIGHT_CH01"), 820.0 + qSin(phase / 4.0) * 90.0, QStringLiteral("lux"), TagQuality::Good, 0},
        ChannelValue{QStringLiteral("VOLTAGE_CH01"), voltageDrop ? 9.1 : 12.2 + qSin(phase / 8.0) * 0.4, QStringLiteral("V"), TagQuality::Good, 0},
        ChannelValue{QStringLiteral("CURRENT_CH01"), 1.3 + qCos(phase / 6.0) * 0.35, QStringLiteral("A"), TagQuality::Good, 0},
        ChannelValue{QStringLiteral("VIBRATION_CH01"), vibrationSpike ? 3.1 + qSin(phase) * 0.5 : 0.65 + qSin(phase / 3.0) * 0.2, QStringLiteral("mm/s"), TagQuality::Good, 0}
    };

    QVector<QVector<double>> rows;
    rows.reserve(16);
    const auto hotspot = (m_sequenceNo % 20) >= 8 && (m_sequenceNo % 20) <= 12;
    for (auto row = 0; row < 16; ++row) {
        QVector<double> rowValues;
        rowValues.reserve(16);
        for (auto column = 0; column < 16; ++column) {
            auto value = 610.0 + row * 4.0 + column * 2.0 + qSin((phase + row + column) / 5.0) * 18.0;
            if (hotspot && row >= 8 && row <= 10 && column >= 9 && column <= 11) {
                value += 320.0;
            }
            rowValues.append(value);
        }
        rows.append(rowValues);
    }
    frame.matrixValues = MatrixFrame::fromRows(QUuid::createUuid(), frame.timestampUtc, rows, frame.frameId, frame.sequenceNo);
    return frame;
}

void UiSnapshotProvider::processFrame(const Monitor::Domain::Measurements::RawMeasurementFrame &frame)
{
    const auto cleaned = m_pipeline.cleanToCleanedValues(frame);
    const auto evaluation = m_alarmService.evaluateWithChanges(cleaned, frame.timestampUtc);
    m_tagService.updateTags(evaluation.states);
    if (frame.matrixValues.has_value()) {
        m_measurementMapService.update(frame.matrixValues.value());
    }

    for (const auto &state : evaluation.states) {
        if (state.numericValue.has_value() || state.boolValue.has_value()) {
            m_historySamples.append(historySampleFromState(state));
        }
    }

    for (const auto &change : evaluation.lifecycleChanges) {
        const auto &alarm = change.alarm;
        appendLog(
            alarm.level == Monitor::Domain::Alarms::AlarmLevel::Alarm
                ? Monitor::Domain::Logs::OperationLogLevel::Error
                : Monitor::Domain::Logs::OperationLogLevel::Warning,
            QStringLiteral("Alarm"),
            QStringLiteral("Alarm.%1").arg(Monitor::Domain::Alarms::toString(alarm.state)),
            alarm.message,
            QStringLiteral("TagId=%1; Value=%2; State=%3")
                .arg(alarm.tagId)
                .arg(formatDouble(alarm.triggerValue))
                .arg(Monitor::Domain::Tags::toString(alarm.alarmType)));
    }

    trimBuffers();
}

void UiSnapshotProvider::appendLog(
    Monitor::Domain::Logs::OperationLogLevel level,
    const QString &category,
    const QString &action,
    const QString &message,
    const std::optional<QString> &detail)
{
    m_operationLogs.prepend(Monitor::Domain::Logs::OperationLog{
        QDateTime::currentDateTimeUtc(),
        level,
        category,
        message,
        action,
        QStringLiteral("QtPresentation"),
        detail,
        std::nullopt,
        static_cast<qint64>(m_operationLogs.size() + 1)
    });
    trimBuffers();
}

void UiSnapshotProvider::trimBuffers()
{
    constexpr auto MaxHistorySamples = 2000;
    constexpr auto MaxLogs = 300;
    if (m_historySamples.size() > MaxHistorySamples) {
        m_historySamples.erase(m_historySamples.begin(), m_historySamples.begin() + (m_historySamples.size() - MaxHistorySamples));
    }
    if (m_operationLogs.size() > MaxLogs) {
        m_operationLogs.resize(MaxLogs);
    }
}

} // namespace Monitor::Application::Services
