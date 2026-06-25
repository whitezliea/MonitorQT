#include "ApplicationLayer.h"

#include "configuration/ConfigurationValidation.h"
#include "configuration/RuntimeOptionsStore.h"
#include "configuration/TagRuntimeConfiguration.h"
#include "configuration/TrendDiagnosisOptions.h"
#include "abstractions/IRawFrameSource.h"
#include "domain/common/DomainCommon.h"
#include "domain/devices/DeviceModels.h"
#include "domain/measurements/MeasurementModels.h"
#include "events/EventBus.h"
#include "pipelines/DataCleanPipeline.h"
#include "queues/ApplicationQueues.h"
#include "runtime/DataSourceHealthMonitor.h"
#include "runtime/MonitoringRuntimeService.h"
#include "runtime/PersistenceRuntimeCoordinator.h"
#include "runtime/RuntimeLifecycleCoordinator.h"
#include "services/AlarmService.h"
#include "services/ChartDataService.h"
#include "services/DashboardService.h"
#include "services/MeasurementMapService.h"
#include "services/OperationLogService.h"
#include "services/RuntimeEventConsumers.h"
#include "services/TagDefinitionCatalog.h"
#include "services/TagService.h"
#include "workers/BatchPersistWorker.h"

#include <QHash>
#include <QSet>
#include <QThread>

#include <atomic>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace Monitor::Application {
namespace {

void addError(QStringList *errors, const QString &message)
{
    errors->append(message);
}

const Monitor::Domain::Tags::TagDefinition *findDefinition(
    const QVector<Monitor::Domain::Tags::TagDefinition> &definitions,
    const QString &tagId)
{
    for (const auto &definition : definitions) {
        if (definition.tagId == tagId) {
            return &definition;
        }
    }

    return nullptr;
}

bool containsChannelMapping(
    const QVector<Monitor::Domain::Tags::TagSourceMapping> &mappings,
    const QString &sourceCode,
    const QString &tagId)
{
    using Monitor::Domain::Tags::SourceType;

    return std::any_of(
        mappings.cbegin(),
        mappings.cend(),
        [&sourceCode, &tagId](const Monitor::Domain::Tags::TagSourceMapping &mapping) {
            return mapping.sourceType == SourceType::Channel
                && mapping.sourceCode.has_value()
                && mapping.sourceCode.value() == sourceCode
                && mapping.tagId == tagId;
        });
}

bool nearlyEqual(double left, double right, double tolerance = 1e-6)
{
    return std::abs(left - right) <= tolerance;
}

QDateTime validationTimestamp(int offsetMs = 0)
{
    const auto baseTicks = 638000000000000000LL;
    return Monitor::Domain::Common::UtcDateTime::fromCSharpTicks(
        baseTicks + static_cast<qint64>(offsetMs) * Monitor::Domain::Common::UtcDateTime::TicksPerMillisecond);
}

Monitor::Domain::Measurements::RawMeasurementFrame createValidationFrame(
    qint64 sequenceNo,
    const QDateTime &timestampUtc,
    double vibration = 0.8,
    Monitor::Domain::Devices::DeviceStatus status = Monitor::Domain::Devices::DeviceStatus::Running,
    Monitor::Domain::Tags::TagQuality quality = Monitor::Domain::Tags::TagQuality::Good)
{
    using Monitor::Domain::Measurements::ChannelValue;
    using Monitor::Domain::Measurements::MatrixFrame;
    using Monitor::Domain::Measurements::RawMeasurementFrame;

    RawMeasurementFrame frame;
    frame.frameId = QUuid::createUuid();
    frame.deviceId = Monitor::Application::Services::TagDefinitionCatalog::defaultDeviceId();
    frame.sequenceNo = sequenceNo;
    frame.timestampUtc = timestampUtc;
    frame.deviceStatus = status;
    frame.channelValues = {
        ChannelValue{QStringLiteral("TEMP_CH01"), 25.0, QStringLiteral("C"), Monitor::Domain::Tags::TagQuality::Good, 0},
        ChannelValue{QStringLiteral("PRESSURE_CH01"), 101.3, QStringLiteral("kPa"), Monitor::Domain::Tags::TagQuality::Good, 0},
        ChannelValue{QStringLiteral("LIGHT_CH01"), 800.0, QStringLiteral("lux"), Monitor::Domain::Tags::TagQuality::Good, 0},
        ChannelValue{QStringLiteral("VOLTAGE_CH01"), 12.1, QStringLiteral("V"), Monitor::Domain::Tags::TagQuality::Good, 0},
        ChannelValue{QStringLiteral("CURRENT_CH01"), 1.2, QStringLiteral("A"), Monitor::Domain::Tags::TagQuality::Good, 0},
        ChannelValue{QStringLiteral("VIBRATION_CH01"), vibration, QStringLiteral("mm/s"), Monitor::Domain::Tags::TagQuality::Good, 0}
    };

    QVector<QVector<double>> matrixRows;
    matrixRows.reserve(16);
    for (auto row = 0; row < 16; ++row) {
        QVector<double> rowValues;
        rowValues.reserve(16);
        for (auto column = 0; column < 16; ++column) {
            rowValues.append(600.0 + row * 2.0 + column);
        }
        matrixRows.append(rowValues);
    }

    frame.matrixValues = MatrixFrame::fromRows(
        QUuid::createUuid(),
        timestampUtc,
        matrixRows,
        frame.frameId,
        sequenceNo);
    frame.errorCode = 0;
    frame.quality = quality;
    return frame;
}

const Monitor::Domain::Tags::CleanedTagValue *findCleanedValue(
    const QVector<Monitor::Domain::Tags::CleanedTagValue> &values,
    const QString &tagId)
{
    const auto it = std::find_if(values.cbegin(), values.cend(), [&tagId](const auto &value) {
        return value.tagId == tagId;
    });

    return it == values.cend() ? nullptr : &(*it);
}

const Monitor::Domain::Tags::TagRuntimeState *findRuntimeState(
    const QVector<Monitor::Domain::Tags::TagRuntimeState> &values,
    const QString &tagId)
{
    const auto it = std::find_if(values.cbegin(), values.cend(), [&tagId](const auto &value) {
        return value.tagId == tagId;
    });

    return it == values.cend() ? nullptr : &(*it);
}

const Monitor::Domain::Alarms::AlarmEvent *findAlarm(
    const QVector<Monitor::Domain::Alarms::AlarmEvent> &alarms,
    const QString &tagId)
{
    const auto it = std::find_if(alarms.cbegin(), alarms.cend(), [&tagId](const auto &alarm) {
        return alarm.tagId == tagId;
    });

    return it == alarms.cend() ? nullptr : &(*it);
}

class ValidationRawFrameSource final : public Monitor::Application::Abstractions::IRawFrameSource
{
public:
    explicit ValidationRawFrameSource(QVector<Monitor::Domain::Measurements::RawMeasurementFrame> frames)
        : m_frames(std::move(frames))
    {
    }

    bool readNextFrame(
        const QDateTime &,
        Monitor::Domain::Measurements::RawMeasurementFrame *frame) override
    {
        if (m_canceled || !frame || m_index >= m_frames.size()) {
            return false;
        }

        *frame = m_frames.at(m_index);
        ++m_index;
        return true;
    }

    void cancel() override
    {
        m_canceled = true;
    }

    void resetCancellation() override
    {
        m_canceled = false;
        m_index = 0;
    }

private:
    QVector<Monitor::Domain::Measurements::RawMeasurementFrame> m_frames;
    int m_index = 0;
    bool m_canceled = false;
};

} // namespace

ApplicationLayerInfo applicationLayerInfo()
{
    return {
        QStringLiteral("MonitorApplication"),
        {
            QStringLiteral("configuration"),
            QStringLiteral("dto"),
            QStringLiteral("events"),
            QStringLiteral("mapping"),
            QStringLiteral("pipelines"),
            QStringLiteral("queues"),
            QStringLiteral("services"),
            QStringLiteral("usecases"),
            QStringLiteral("workers")
        },
        {
            QStringLiteral("MonitorDomain"),
            QStringLiteral("QtCore")
        },
        {
            QStringLiteral("QtWidgets"),
            QStringLiteral("QtSql concrete repositories"),
            QStringLiteral("Simulator internals")
        }
    };
}

QStringList validateApplicationLayer()
{
    QStringList errors;

    try {
        const auto options = Configuration::MonitorRuntimeOptions();
        Configuration::ConfigurationValidation::validateRuntimeOptions(options);
        if (options.dataGenerateIntervalMs != 500 ||
            options.dataSourceTimeoutPeriods != 3 ||
            options.dataSourceTimeoutMs() != 1500 ||
            options.uiRefreshIntervalMs != 1000 ||
            options.historyBatchIntervalMs != 5000 ||
            options.alarmBatchIntervalMs != 5000 ||
            options.operationLogBatchIntervalMs != 5000 ||
            options.historyRetentionDays != 30 ||
            options.trendPointCount(1) != 120 ||
            options.trendPointCount(5) != 600 ||
            options.trendPointCount(30) != 3600 ||
            options.trendBufferCapacity() != 3600) {
            addError(&errors, QStringLiteral("MonitorRuntimeOptions defaults are not aligned with the WPF source."));
        }

        Configuration::RuntimeOptionsStore runtimeStore(options);
        auto changedOptions = runtimeStore.snapshot();
        changedOptions.uiRefreshIntervalMs = 250;
        runtimeStore.replace(changedOptions);
        if (runtimeStore.snapshot().uiRefreshIntervalMs != 250) {
            addError(&errors, QStringLiteral("RuntimeOptionsStore did not replace its snapshot."));
        }

        const auto definitions = Services::TagDefinitionCatalog::createDefaults();
        if (definitions.size() != 22) {
            addError(&errors, QStringLiteral("TagDefinitionCatalog must provide exactly 22 default definitions."));
        }

        QSet<QString> tagIds;
        for (const auto &definition : definitions) {
            if (tagIds.contains(definition.tagId)) {
                addError(&errors, QStringLiteral("Duplicate TagDefinition: %1").arg(definition.tagId));
            }
            tagIds.insert(definition.tagId);
        }

        const QStringList requiredTags = {
            QStringLiteral("DEVICE.STATUS"),
            QStringLiteral("DEVICE.ONLINE"),
            QStringLiteral("DEVICE.ERROR_CODE"),
            QStringLiteral("DEVICE.QUALITY"),
            QStringLiteral("DEVICE.SEQUENCE_NO"),
            QStringLiteral("DEVICE.FRAME_INTERVAL_MS"),
            QStringLiteral("DEVICE.FRAME_LOSS_COUNT"),
            QStringLiteral("MEAS.TEMP.CH01"),
            QStringLiteral("MEAS.PRESSURE.CH01"),
            QStringLiteral("MEAS.LIGHT.CH01"),
            QStringLiteral("MEAS.VOLTAGE.CH01"),
            QStringLiteral("MEAS.CURRENT.CH01"),
            QStringLiteral("MEAS.VIBRATION.CH01"),
            QStringLiteral("MEAS.POWER.CH01"),
            QStringLiteral("MEAS.LOAD_RATIO.CH01"),
            QStringLiteral("MATRIX.LIGHT.AVG"),
            QStringLiteral("MATRIX.LIGHT.MAX"),
            QStringLiteral("MATRIX.LIGHT.MIN"),
            QStringLiteral("MATRIX.LIGHT.UNIFORMITY"),
            QStringLiteral("MATRIX.LIGHT.ABNORMAL_COUNT"),
            QStringLiteral("MATRIX.LIGHT.HOTSPOT_ROW"),
            QStringLiteral("MATRIX.LIGHT.HOTSPOT_COL")
        };

        for (const auto &tagId : requiredTags) {
            if (!tagIds.contains(tagId)) {
                addError(&errors, QStringLiteral("Required default TagDefinition is missing: %1").arg(tagId));
            }
        }

        const auto mappings = Services::TagDefinitionCatalog::createSourceMappings();
        if (mappings.size() != 22) {
            addError(&errors, QStringLiteral("TagDefinitionCatalog must provide exactly 22 source mappings."));
        }

        const QHash<QString, QString> requiredChannelMappings = {
            {QStringLiteral("TEMP_CH01"), QStringLiteral("MEAS.TEMP.CH01")},
            {QStringLiteral("PRESSURE_CH01"), QStringLiteral("MEAS.PRESSURE.CH01")},
            {QStringLiteral("LIGHT_CH01"), QStringLiteral("MEAS.LIGHT.CH01")},
            {QStringLiteral("VOLTAGE_CH01"), QStringLiteral("MEAS.VOLTAGE.CH01")},
            {QStringLiteral("CURRENT_CH01"), QStringLiteral("MEAS.CURRENT.CH01")},
            {QStringLiteral("VIBRATION_CH01"), QStringLiteral("MEAS.VIBRATION.CH01")}
        };

        for (auto it = requiredChannelMappings.cbegin(); it != requiredChannelMappings.cend(); ++it) {
            if (!containsChannelMapping(mappings, it.key(), it.value())) {
                addError(&errors, QStringLiteral("Required channel mapping is missing: %1 -> %2").arg(it.key(), it.value()));
            }
        }

        QVector<Configuration::TagRuntimeConfiguration> configurations;
        configurations.reserve(definitions.size());
        for (const auto &definition : definitions) {
            auto configuration = Configuration::TagRuntimeConfiguration::fromDefinition(definition);
            configurations.append(configuration);
        }

        Configuration::TagRuntimeConfigurationStore configurationStore(configurations);
        if (configurationStore.snapshot().size() != 22 ||
            configurationStore.get(QStringLiteral("MEAS.TEMP.CH01")).revision != 0) {
            addError(&errors, QStringLiteral("TagRuntimeConfigurationStore default snapshot is not aligned."));
        }

        const auto *tempDefinition = findDefinition(definitions, QStringLiteral("MEAS.TEMP.CH01"));
        if (!tempDefinition) {
            addError(&errors, QStringLiteral("MEAS.TEMP.CH01 definition is required for validation."));
        } else {
            Configuration::ConfigurationValidation::validateTag(
                *tempDefinition,
                Configuration::TagRuntimeConfiguration::fromDefinition(*tempDefinition));
        }

        auto replacement = configurationStore.get(QStringLiteral("MEAS.TEMP.CH01"));
        replacement.warningHigh = 55.0;
        configurations[0] = configurationStore.get(definitions.first().tagId);
        for (auto &configuration : configurations) {
            if (configuration.tagId == replacement.tagId) {
                configuration = replacement;
                break;
            }
        }
        configurationStore.replace(configurations);
        const auto replaced = configurationStore.get(QStringLiteral("MEAS.TEMP.CH01"));
        if (replaced.revision != 1 || !replaced.warningHigh.has_value() || replaced.warningHigh.value() != 55.0) {
            addError(&errors, QStringLiteral("TagRuntimeConfigurationStore replacement revision behavior is not aligned."));
        }

        if (!tempDefinition) {
            addError(&errors, QStringLiteral("MEAS.TEMP.CH01 definition is required for validation."));
        } else {
            auto invalid = Configuration::TagRuntimeConfiguration::fromDefinition(*tempDefinition);
            invalid.alarmLow = 10.0;
            invalid.warningLow = 5.0;
            auto rejectedInvalidThresholdOrder = false;
            try {
                Configuration::ConfigurationValidation::validateTag(*tempDefinition, invalid);
            } catch (const std::exception &) {
                rejectedInvalidThresholdOrder = true;
            }

            if (!rejectedInvalidThresholdOrder) {
                addError(&errors, QStringLiteral("ConfigurationValidation must reject AlarmLow > WarningLow."));
            }
        }

        Configuration::TrendDiagnosisOptions trendDiagnosisOptions;
        trendDiagnosisOptions.validate();

        Pipelines::DataCleanPipeline pipeline(definitions, mappings);
        const auto firstFrame = createValidationFrame(1, validationTimestamp());
        const auto cleanedFirst = pipeline.cleanToCleanedValues(firstFrame);
        if (cleanedFirst.size() != 22) {
            addError(&errors, QStringLiteral("DataCleanPipeline must emit exactly 22 cleaned values for the default frame."));
        }

        const auto requireNumeric = [&errors](const auto &values, const QString &tagId, double expected, double tolerance = 1e-6) {
            const auto *value = findCleanedValue(values, tagId);
            if (!value || !value->numericValue.has_value() || !nearlyEqual(value->numericValue.value(), expected, tolerance)) {
                addError(&errors, QStringLiteral("Unexpected numeric value for %1.").arg(tagId));
            }
        };
        const auto requireQuality = [&errors](const auto &values, const QString &tagId, Monitor::Domain::Tags::TagQuality expected) {
            const auto *value = findCleanedValue(values, tagId);
            if (!value || value->quality != expected) {
                addError(&errors, QStringLiteral("Unexpected quality for %1.").arg(tagId));
            }
        };

        requireNumeric(cleanedFirst, QStringLiteral("DEVICE.FRAME_INTERVAL_MS"), 0.0);
        requireNumeric(cleanedFirst, QStringLiteral("DEVICE.FRAME_LOSS_COUNT"), 0.0);
        requireNumeric(cleanedFirst, QStringLiteral("MEAS.POWER.CH01"), 14.52);
        requireNumeric(cleanedFirst, QStringLiteral("MEAS.LOAD_RATIO.CH01"), 24.0);
        requireNumeric(cleanedFirst, QStringLiteral("MATRIX.LIGHT.AVG"), 622.5);
        requireNumeric(cleanedFirst, QStringLiteral("MATRIX.LIGHT.MIN"), 600.0);
        requireNumeric(cleanedFirst, QStringLiteral("MATRIX.LIGHT.MAX"), 645.0);
        requireNumeric(cleanedFirst, QStringLiteral("MATRIX.LIGHT.UNIFORMITY"), 600.0 / 645.0);
        requireNumeric(cleanedFirst, QStringLiteral("MATRIX.LIGHT.ABNORMAL_COUNT"), 0.0);
        requireNumeric(cleanedFirst, QStringLiteral("MATRIX.LIGHT.HOTSPOT_ROW"), 15.0);
        requireNumeric(cleanedFirst, QStringLiteral("MATRIX.LIGHT.HOTSPOT_COL"), 15.0);
        requireQuality(cleanedFirst, QStringLiteral("MEAS.POWER.CH01"), Monitor::Domain::Tags::TagQuality::Good);

        const auto secondFrame = createValidationFrame(3, validationTimestamp(1000), 6.0);
        const auto cleanedSecond = pipeline.cleanToCleanedValues(secondFrame);
        requireNumeric(cleanedSecond, QStringLiteral("DEVICE.FRAME_INTERVAL_MS"), 1000.0);
        requireNumeric(cleanedSecond, QStringLiteral("DEVICE.FRAME_LOSS_COUNT"), 1.0);

        const auto offlineFrame = createValidationFrame(
            4,
            validationTimestamp(1500),
            0.8,
            Monitor::Domain::Devices::DeviceStatus::Offline);
        const auto cleanedOffline = pipeline.cleanToCleanedValues(offlineFrame);
        requireQuality(cleanedOffline, QStringLiteral("MEAS.POWER.CH01"), Monitor::Domain::Tags::TagQuality::Offline);

        Pipelines::DataCleanPipeline alarmPipeline(definitions, mappings);
        const auto alarmFrame = createValidationFrame(20, validationTimestamp(2000), 6.0);
        const auto alarmCleaned = alarmPipeline.cleanToCleanedValues(alarmFrame);
        Services::AlarmService alarmService(definitions);
        const auto alarmResult = alarmService.evaluateWithChanges(alarmCleaned, alarmFrame.timestampUtc);
        const auto *vibrationState = findRuntimeState(alarmResult.states, QStringLiteral("MEAS.VIBRATION.CH01"));
        if (!vibrationState || vibrationState->alarmState != Monitor::Domain::Tags::TagAlarmState::AlarmHigh) {
            addError(&errors, QStringLiteral("AlarmService must evaluate MEAS.VIBRATION.CH01 as AlarmHigh."));
        }

        const auto activeAlarms = alarmService.currentAlarms();
        const auto *vibrationAlarm = findAlarm(activeAlarms, QStringLiteral("MEAS.VIBRATION.CH01"));
        if (!vibrationAlarm || vibrationAlarm->level != Monitor::Domain::Alarms::AlarmLevel::Alarm) {
            addError(&errors, QStringLiteral("AlarmService must raise an active vibration alarm."));
        } else {
            Monitor::Domain::Alarms::AlarmEvent acknowledgedAlarm;
            if (!alarmService.acknowledge(vibrationAlarm->alarmId, validationTimestamp(2010), &acknowledgedAlarm) ||
                acknowledgedAlarm.state != Monitor::Domain::Alarms::AlarmState::Acknowledged) {
                addError(&errors, QStringLiteral("AlarmService must acknowledge active alarms."));
            }
        }

        const auto recoverFrame = createValidationFrame(21, validationTimestamp(2500), 0.8);
        const auto recoverCleaned = alarmPipeline.cleanToCleanedValues(recoverFrame);
        const auto recoverResult = alarmService.evaluateWithChanges(recoverCleaned, recoverFrame.timestampUtc);
        const auto recovered = std::any_of(
            recoverResult.lifecycleChanges.cbegin(),
            recoverResult.lifecycleChanges.cend(),
            [](const Monitor::Application::Dtos::AlarmLifecycleChange &change) {
                return change.changeType == Monitor::Application::Dtos::AlarmLifecycleChangeType::Recovered
                    && change.alarm.tagId == QStringLiteral("MEAS.VIBRATION.CH01")
                    && change.alarm.state == Monitor::Domain::Alarms::AlarmState::Recovered;
            });
        if (!recovered || !alarmService.currentAlarms().isEmpty()) {
            addError(&errors, QStringLiteral("AlarmService must recover the vibration alarm when the value returns to normal."));
        }

        Services::AlarmService stateProjector(definitions);
        Services::TagService tagService(2);
        Pipelines::DataCleanPipeline cachePipeline(definitions, mappings);
        for (auto index = 0; index < 3; ++index) {
            auto frame = createValidationFrame(30 + index, validationTimestamp(3000 + index * 500));
            frame.channelValues[0].value = 20.0 + index;
            const auto cleaned = cachePipeline.cleanToCleanedValues(frame);
            tagService.updateTags(stateProjector.evaluate(cleaned, frame.timestampUtc));
        }

        const auto tagSnapshot = tagService.snapshot();
        if (tagSnapshot.currentValues.size() != 22 ||
            !tagSnapshot.recentBuffers.contains(QStringLiteral("MEAS.TEMP.CH01")) ||
            tagSnapshot.recentBuffers.value(QStringLiteral("MEAS.TEMP.CH01")).size() != 2 ||
            !nearlyEqual(tagSnapshot.recentBuffers.value(QStringLiteral("MEAS.TEMP.CH01")).first().value, 21.0)) {
            addError(&errors, QStringLiteral("TagCache must store current values and trim trend buffers by capacity."));
        }

        Services::ChartDataService chartDataService(&tagService);
        const auto trendSeries = chartDataService.trendSeries(QStringLiteral("MEAS.TEMP.CH01"), 10);
        if (trendSeries.points.size() != 2 ||
            trendSeries.requestedPointCount != 10 ||
            trendSeries.isWindowComplete()) {
            addError(&errors, QStringLiteral("ChartDataService must build trend series from TagCache buffers."));
        }

        Services::DashboardService dashboardService(&tagService, &alarmService);
        const auto dashboardSnapshot = dashboardService.buildSnapshot(
            tagSnapshot,
            alarmService.currentAlarms(),
            validationTimestamp(5000));
        if (dashboardSnapshot.totalTagCount != 22 ||
            dashboardSnapshot.badQualityCount != 0 ||
            dashboardSnapshot.sequenceNo != 32) {
            addError(&errors, QStringLiteral("DashboardService must aggregate tag and alarm snapshots."));
        }

        Services::MeasurementMapService measurementMapService;
        measurementMapService.update(firstFrame.matrixValues.value());
        const auto matrixAnalysis = measurementMapService.latestAnalysis();
        const auto matrixSnapshot = measurementMapService.latestSnapshot();
        if (!matrixAnalysis.has_value() ||
            matrixAnalysis->frame.rows != 16 ||
            matrixAnalysis->frame.columns != 16 ||
            !nearlyEqual(matrixAnalysis->statistics.averageValue, 622.5) ||
            !matrixAnalysis->abnormalPoints.isEmpty() ||
            matrixAnalysis->qualityState != Monitor::Application::Dtos::MatrixQualityState::Good) {
            addError(&errors, QStringLiteral("MeasurementMapService must analyze the latest matrix frame."));
        }
        if (!matrixSnapshot.has_value() ||
            matrixSnapshot->cells.size() != 256 ||
            !nearlyEqual(matrixSnapshot->scaleRange.minValue, 600.0) ||
            !nearlyEqual(matrixSnapshot->scaleRange.maxValue, 645.0)) {
            addError(&errors, QStringLiteral("MeasurementMapService must build matrix preview cells and scale range."));
        }

        {
            EventBus eventBus;
            auto isolatedHandlerRan = false;
            auto trailingHandlerRan = false;
            eventBus.registerHandler(
                QStringLiteral("RawFrameReceivedEvent"),
                QStringLiteral("IsolatedThrowingConsumer"),
                EventHandlerFailurePolicy::Isolated,
                10,
                [](const Monitor::Application::Events::ApplicationEvent &) {
                    throw std::runtime_error("isolated failure");
                });
            eventBus.registerHandler(
                QStringLiteral("RawFrameReceivedEvent"),
                QStringLiteral("TrailingConsumer"),
                EventHandlerFailurePolicy::Critical,
                20,
                [&trailingHandlerRan, &isolatedHandlerRan](const Monitor::Application::Events::ApplicationEvent &) {
                    isolatedHandlerRan = true;
                    trailingHandlerRan = true;
                });
            QStringList publishErrors;
            if (!eventBus.publish(Monitor::Application::Events::RawFrameReceivedEvent{firstFrame}, &publishErrors) ||
                publishErrors.isEmpty() ||
                !isolatedHandlerRan ||
                !trailingHandlerRan) {
                addError(&errors, QStringLiteral("EventBus must isolate non-critical handler failures and continue dispatching."));
            }

            EventBus criticalBus;
            criticalBus.registerHandler(
                QStringLiteral("RawFrameReceivedEvent"),
                QStringLiteral("CriticalThrowingConsumer"),
                EventHandlerFailurePolicy::Critical,
                10,
                [](const Monitor::Application::Events::ApplicationEvent &) {
                    throw std::runtime_error("critical failure");
                });
            QStringList criticalErrors;
            if (criticalBus.publish(Monitor::Application::Events::RawFrameReceivedEvent{firstFrame}, &criticalErrors) ||
                criticalErrors.isEmpty()) {
                addError(&errors, QStringLiteral("EventBus must stop dispatching when a critical handler fails."));
            }
        }

        Configuration::MonitorRuntimeOptions stage6Options;
        stage6Options.dataGenerateIntervalMs = 1;
        stage6Options.dataSourceTimeoutPeriods = 2;
        Services::TagService runtimeTagService(8);
        Services::MeasurementMapService runtimeMeasurementMapService;
        Queues::HistorySampleQueue historyQueue;
        Queues::AlarmEventQueue alarmEventQueue;
        Queues::OperationLogQueue operationLogQueue;
        Services::OperationLogService operationLogService(&operationLogQueue);
        Services::TagCacheConsumer tagCacheConsumer(&runtimeTagService);
        Services::MeasurementMapFrameConsumer measurementMapFrameConsumer(&runtimeMeasurementMapService);
        Services::HistoryRuntimeStateConsumer historyRuntimeStateConsumer(&historyQueue, definitions);
        Services::AlarmEventConsumer alarmEventConsumer(&alarmEventQueue);
        Services::AlarmOperationLogConsumer alarmOperationLogConsumer(&operationLogService);
        Services::DataSourceHealthOperationLogConsumer dataSourceHealthOperationLogConsumer(&operationLogService);
        EventBus runtimeEventBus;
        QStringList registrationErrors;
        if (!Services::registerDefaultRuntimeConsumers(
                &runtimeEventBus,
                &tagCacheConsumer,
                &measurementMapFrameConsumer,
                &historyRuntimeStateConsumer,
                &alarmEventConsumer,
                &alarmOperationLogConsumer,
                &dataSourceHealthOperationLogConsumer,
                &registrationErrors) ||
            !registrationErrors.isEmpty()) {
            addError(&errors, QStringLiteral("Runtime event consumers must register successfully."));
        }

        Pipelines::DataCleanPipeline runtimePipeline(definitions, mappings);
        Services::AlarmService runtimeAlarmService(definitions);
        const auto runtimeFrame = createValidationFrame(100, validationTimestamp(6000), 6.0);
        ValidationRawFrameSource runtimeSource({runtimeFrame});
        Runtime::DataSourceHealthMonitor healthMonitor;
        Runtime::MonitoringRuntimeService monitoringRuntime(
            &runtimeSource,
            &runtimePipeline,
            &runtimeAlarmService,
            &runtimeEventBus,
            stage6Options,
            &healthMonitor);
        QStringList runtimeErrors;
        if (!monitoringRuntime.processFrame(runtimeFrame, &runtimeErrors) || !runtimeErrors.isEmpty()) {
            addError(&errors, QStringLiteral("MonitoringRuntimeService must process a raw frame through the event chain."));
        }

        const auto runtimeTagSnapshot = runtimeTagService.snapshot();
        if (runtimeTagSnapshot.currentValues.size() != 22 ||
            !runtimeMeasurementMapService.latestAnalysis().has_value() ||
            alarmEventQueue.size() != 1 ||
            operationLogQueue.size() != 1 ||
            historyQueue.size() == 0) {
            addError(&errors, QStringLiteral("Runtime consumers must update tag cache, matrix snapshot, alarm queue, operation log queue and history queue."));
        }

        QThread::msleep(4);
        QStringList timeoutErrors;
        if (!monitoringRuntime.publishOfflineStatesIfTimedOut(QDateTime::currentDateTimeUtc(), &timeoutErrors) ||
            !timeoutErrors.isEmpty()) {
            addError(&errors, QStringLiteral("MonitoringRuntimeService must publish offline states when the data source times out."));
        }
        const auto offlineSnapshot = runtimeTagService.snapshot();
        const auto *offlinePower = findRuntimeState(offlineSnapshot.currentValues, QStringLiteral("MEAS.POWER.CH01"));
        if (healthMonitor.status().state != Runtime::DataSourceHealthState::TimedOut ||
            !offlinePower ||
            offlinePower->quality != Monitor::Domain::Tags::TagQuality::Offline ||
            operationLogQueue.size() < 2) {
            addError(&errors, QStringLiteral("DataSourceHealthMonitor timeout must publish operation log and offline runtime states."));
        }

        Queues::HistorySampleQueue workerQueue;
        QVector<Monitor::Domain::Tags::TagValue> persistedHistory;
        Workers::BatchPersistWorker<Monitor::Domain::Tags::TagValue> historyWorker(
            QStringLiteral("History"),
            &workerQueue,
            20,
            2,
            [&persistedHistory](const QVector<Monitor::Domain::Tags::TagValue> &items) {
                persistedHistory += items;
            });
        historyWorker.start();
        workerQueue.enqueue(Monitor::Domain::Tags::TagValue{
            QStringLiteral("MEAS.TEMP.CH01"),
            20.0,
            validationTimestamp(7000),
            Monitor::Domain::Tags::TagQuality::Good,
            Monitor::Domain::Tags::TagAlarmState::Normal,
            QStringLiteral("validation"),
            1
        });
        workerQueue.enqueue(Monitor::Domain::Tags::TagValue{
            QStringLiteral("MEAS.TEMP.CH01"),
            21.0,
            validationTimestamp(7500),
            Monitor::Domain::Tags::TagQuality::Good,
            Monitor::Domain::Tags::TagAlarmState::Normal,
            QStringLiteral("validation"),
            2
        });
        QThread::msleep(50);
        historyWorker.stop();
        if (persistedHistory.size() < 2 ||
            historyWorker.status().state != Workers::PersistWorkerState::Stopped) {
            addError(&errors, QStringLiteral("BatchPersistWorker must flush queued items by batch size or shutdown."));
        }

        std::atomic_int runtimeIterations = 0;
        Runtime::RuntimeLifecycleCoordinator lifecycle([&runtimeIterations](std::atomic_bool &stopRequested) {
            while (!stopRequested.load()) {
                ++runtimeIterations;
                QThread::msleep(5);
            }
        });
        if (!lifecycle.start()) {
            addError(&errors, QStringLiteral("RuntimeLifecycleCoordinator must start a runtime thread."));
        }
        QThread::msleep(25);
        if (!lifecycle.stop() ||
            runtimeIterations.load() == 0 ||
            lifecycle.status().state != Runtime::RuntimeLifecycleState::Stopped) {
            addError(&errors, QStringLiteral("RuntimeLifecycleCoordinator must stop and join the runtime thread cleanly."));
        }

        Queues::OperationLogQueue controllerLogQueue;
        Services::OperationLogService controllerLogService(&controllerLogQueue);
        Queues::HistorySampleQueue controllerHistoryQueue;
        QVector<Monitor::Domain::Tags::TagValue> controllerPersistedHistory;
        Workers::BatchPersistWorker<Monitor::Domain::Tags::TagValue> controllerHistoryWorker(
            QStringLiteral("History"),
            &controllerHistoryQueue,
            20,
            10,
            [&controllerPersistedHistory](const QVector<Monitor::Domain::Tags::TagValue> &items) {
                controllerPersistedHistory += items;
            });
        Runtime::PersistenceRuntimeCoordinator persistenceRuntime({&controllerHistoryWorker});
        if (!persistenceRuntime.start() ||
            persistenceRuntime.status().state != Runtime::PersistenceRuntimeState::Running) {
            addError(&errors, QStringLiteral("PersistenceRuntimeCoordinator must start configured workers."));
        }
        controllerHistoryQueue.enqueue(Monitor::Domain::Tags::TagValue{
            QStringLiteral("MEAS.CURRENT.CH01"),
            1.0,
            validationTimestamp(8000),
            Monitor::Domain::Tags::TagQuality::Good,
            Monitor::Domain::Tags::TagAlarmState::Normal,
            QStringLiteral("validation"),
            3
        });
        if (!persistenceRuntime.flushHistory()) {
            addError(&errors, QStringLiteral("PersistenceRuntimeCoordinator must flush the History worker explicitly."));
        }
        persistenceRuntime.stop();
        if (controllerPersistedHistory.isEmpty() ||
            persistenceRuntime.status().state != Runtime::PersistenceRuntimeState::Stopped) {
            addError(&errors, QStringLiteral("PersistenceRuntimeCoordinator must stop workers after flushing."));
        }
    } catch (const std::exception &exception) {
        addError(&errors, QStringLiteral("Application validation threw unexpectedly: %1").arg(QString::fromUtf8(exception.what())));
    }

    return errors;
}

} // namespace Monitor::Application
