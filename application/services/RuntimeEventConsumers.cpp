#include "RuntimeEventConsumers.h"

#include "domain/common/DomainCommon.h"

#include <algorithm>
#include <optional>
#include <stdexcept>

namespace Monitor::Application::Services {
namespace {

using Monitor::Application::EventHandlerFailurePolicy;
using Monitor::Application::Events::AlarmAcknowledgedEvent;
using Monitor::Application::Events::AlarmRaisedEvent;
using Monitor::Application::Events::AlarmRecoveredEvent;
using Monitor::Application::Events::AlarmUpdatedEvent;
using Monitor::Application::Events::DataSourceRecoveredEvent;
using Monitor::Application::Events::DataSourceTimedOutEvent;
using Monitor::Application::Events::RawFrameReceivedEvent;
using Monitor::Application::Events::TagRuntimeStatesProducedEvent;

template <typename TEvent>
const TEvent *asEvent(const Monitor::Application::Events::ApplicationEvent &event)
{
    return std::get_if<TEvent>(&event);
}

template <typename TEvent, typename TConsumer>
bool registerHandler(
    Monitor::Application::EventBus *eventBus,
    const QString &consumerName,
    EventHandlerFailurePolicy policy,
    int order,
    TConsumer *consumer,
    QStringList *errors)
{
    if (!eventBus || !consumer) {
        if (errors) {
            errors->append(QStringLiteral("Cannot register %1 because the EventBus or consumer is null.").arg(consumerName));
        }
        return false;
    }

    const auto name = Monitor::Application::Events::eventName(TEvent{});
    return eventBus->registerHandler(
        name,
        consumerName,
        policy,
        order,
        [consumer](const Monitor::Application::Events::ApplicationEvent &event) {
            consumer->handle(event);
        });
}

QHash<QString, Monitor::Application::Configuration::TagRuntimeConfiguration> configurationSnapshot(
    const QVector<Monitor::Domain::Tags::TagDefinition> &definitions)
{
    QHash<QString, Monitor::Application::Configuration::TagRuntimeConfiguration> result;
    for (const auto &definition : definitions) {
        const auto configuration = Monitor::Application::Configuration::TagRuntimeConfiguration::fromDefinition(definition);
        result.insert(configuration.tagId, configuration);
    }

    return result;
}

std::optional<Monitor::Domain::Alarms::AlarmEvent> alarmFromEvent(
    const Monitor::Application::Events::ApplicationEvent &event)
{
    if (const auto *raised = asEvent<AlarmRaisedEvent>(event)) {
        return raised->alarm;
    }
    if (const auto *updated = asEvent<AlarmUpdatedEvent>(event)) {
        return updated->alarm;
    }
    if (const auto *recovered = asEvent<AlarmRecoveredEvent>(event)) {
        return recovered->alarm;
    }
    if (const auto *acknowledged = asEvent<AlarmAcknowledgedEvent>(event)) {
        return acknowledged->alarm;
    }
    return std::nullopt;
}

} // namespace

TagCacheConsumer::TagCacheConsumer(TagService *tagService)
    : m_tagService(tagService)
{
    if (!m_tagService) {
        throw std::invalid_argument("TagService must not be null.");
    }
}

void TagCacheConsumer::handle(const Monitor::Application::Events::ApplicationEvent &event) const
{
    const auto *typed = asEvent<TagRuntimeStatesProducedEvent>(event);
    if (!typed) {
        return;
    }

    m_tagService->updateTags(typed->states);
}

MeasurementMapFrameConsumer::MeasurementMapFrameConsumer(MeasurementMapService *measurementMapService)
    : m_measurementMapService(measurementMapService)
{
    if (!m_measurementMapService) {
        throw std::invalid_argument("MeasurementMapService must not be null.");
    }
}

void MeasurementMapFrameConsumer::handle(const Monitor::Application::Events::ApplicationEvent &event) const
{
    const auto *typed = asEvent<RawFrameReceivedEvent>(event);
    if (!typed || !typed->frame.matrixValues.has_value()) {
        return;
    }

    auto matrix = typed->frame.matrixValues.value();
    matrix.sourceFrameId = typed->frame.frameId;
    matrix.sequenceNo = typed->frame.sequenceNo;
    m_measurementMapService->update(matrix);
}

HistoryRuntimeStateConsumer::HistoryRuntimeStateConsumer(
    Monitor::Application::Queues::HistorySampleQueue *queue,
    const QVector<Monitor::Domain::Tags::TagDefinition> &definitions)
    : m_queue(queue),
      m_configurations(configurationSnapshot(definitions))
{
    if (!m_queue) {
        throw std::invalid_argument("HistorySampleQueue must not be null.");
    }
}

void HistoryRuntimeStateConsumer::handle(const Monitor::Application::Events::ApplicationEvent &event)
{
    const auto *typed = asEvent<TagRuntimeStatesProducedEvent>(event);
    if (!typed) {
        return;
    }

    for (const auto &state : typed->states) {
        Monitor::Domain::Common::UtcDateTime::require(state.timestampUtc, QStringLiteral("state.timestampUtc"));
        const auto configurationIt = m_configurations.constFind(state.tagId);
        if (configurationIt == m_configurations.cend() || !shouldPersist(state, configurationIt.value())) {
            continue;
        }

        m_queue->enqueue(Monitor::Domain::Tags::TagValue{
            state.tagId,
            state.numericValue.value(),
            state.timestampUtc,
            state.quality,
            state.alarmState,
            state.sourceFrameId.toString(QUuid::WithoutBraces),
            state.sequenceNo
        });
        m_lastPersisted.insert(state.tagId, {
            state.timestampUtc,
            state.quality,
            state.alarmState,
            configurationIt.value().revision
        });
    }
}

bool HistoryRuntimeStateConsumer::shouldPersist(
    const Monitor::Domain::Tags::TagRuntimeState &state,
    const Monitor::Application::Configuration::TagRuntimeConfiguration &configuration)
{
    if (!state.numericValue.has_value() || !configuration.isHistorized) {
        return false;
    }

    const auto lastIt = m_lastPersisted.constFind(state.tagId);
    if (lastIt == m_lastPersisted.cend() || lastIt.value().revision != configuration.revision) {
        return true;
    }

    if (state.timestampUtc < lastIt.value().timestampUtc) {
        m_lastPersisted.remove(state.tagId);
        return true;
    }

    if (state.quality != lastIt.value().quality || state.alarmState != lastIt.value().alarmState) {
        return true;
    }

    return lastIt.value().timestampUtc.msecsTo(state.timestampUtc) >= configuration.historyIntervalMs;
}

AlarmEventConsumer::AlarmEventConsumer(Monitor::Application::Queues::AlarmEventQueue *queue)
    : m_queue(queue)
{
    if (!m_queue) {
        throw std::invalid_argument("AlarmEventQueue must not be null.");
    }
}

void AlarmEventConsumer::handle(const Monitor::Application::Events::ApplicationEvent &event) const
{
    const auto alarm = alarmFromEvent(event);
    if (alarm.has_value()) {
        m_queue->enqueue(alarm.value());
    }
}

AlarmOperationLogConsumer::AlarmOperationLogConsumer(OperationLogService *operationLogService)
    : m_operationLogService(operationLogService)
{
    if (!m_operationLogService) {
        throw std::invalid_argument("OperationLogService must not be null.");
    }
}

void AlarmOperationLogConsumer::handle(const Monitor::Application::Events::ApplicationEvent &event) const
{
    if (const auto *raised = asEvent<AlarmRaisedEvent>(event)) {
        write(raised->alarm, QStringLiteral("Alarm.Raised"), QStringLiteral("Alarm raised."));
    } else if (const auto *updated = asEvent<AlarmUpdatedEvent>(event)) {
        write(updated->alarm, QStringLiteral("Alarm.Updated"), QStringLiteral("Alarm level updated."));
    } else if (const auto *recovered = asEvent<AlarmRecoveredEvent>(event)) {
        write(recovered->alarm, QStringLiteral("Alarm.Recovered"), QStringLiteral("Alarm recovered."));
    } else if (const auto *acknowledged = asEvent<AlarmAcknowledgedEvent>(event)) {
        write(acknowledged->alarm, QStringLiteral("Alarm.Acknowledged"), QStringLiteral("Alarm acknowledged."));
    }
}

void AlarmOperationLogConsumer::write(
    const Monitor::Domain::Alarms::AlarmEvent &alarm,
    const QString &action,
    const QString &message) const
{
    const auto detail = QStringLiteral("TagId=%1; Level=%2; Type=%3; State=%4; Value=%5; CloseReason=%6; Message=%7")
        .arg(
            alarm.tagId,
            Monitor::Domain::Alarms::toString(alarm.level),
            Monitor::Domain::Tags::toString(alarm.alarmType),
            Monitor::Domain::Alarms::toString(alarm.state),
            QString::number(alarm.triggerValue, 'f', 3),
            alarm.closeReason.value_or(QString()),
            alarm.message);
    const auto level = alarm.level == Monitor::Domain::Alarms::AlarmLevel::Alarm
        ? Monitor::Domain::Logs::OperationLogLevel::Error
        : Monitor::Domain::Logs::OperationLogLevel::Warning;
    m_operationLogService->write(
        level,
        QStringLiteral("Alarm"),
        action,
        QStringLiteral("AlarmOperationLogConsumer"),
        message,
        detail,
        alarm.alarmId.toString(QUuid::WithoutBraces));
}

DataSourceHealthOperationLogConsumer::DataSourceHealthOperationLogConsumer(OperationLogService *operationLogService)
    : m_operationLogService(operationLogService)
{
    if (!m_operationLogService) {
        throw std::invalid_argument("OperationLogService must not be null.");
    }
}

void DataSourceHealthOperationLogConsumer::handle(const Monitor::Application::Events::ApplicationEvent &event) const
{
    if (const auto *timedOut = asEvent<DataSourceTimedOutEvent>(event)) {
        const auto detail = QStringLiteral("FrameId=%1; SequenceNo=%2; LastFrameUtc=%3; TimedOutAtUtc=%4")
            .arg(
                timedOut->lastFrameId.toString(QUuid::WithoutBraces),
                QString::number(timedOut->lastSequenceNo),
                timedOut->lastFrameTimeUtc.toString(Qt::ISODateWithMs),
                timedOut->timedOutAtUtc.toString(Qt::ISODateWithMs));
        m_operationLogService->write(
            Monitor::Domain::Logs::OperationLogLevel::Error,
            QStringLiteral("Acquisition"),
            QStringLiteral("DataSource.TimedOut"),
            QStringLiteral("DataSourceHealthOperationLogConsumer"),
            QStringLiteral("Data source stopped producing frames."),
            detail);
        return;
    }

    if (const auto *recovered = asEvent<DataSourceRecoveredEvent>(event)) {
        const auto detail = QStringLiteral("FrameId=%1; SequenceNo=%2; RecoveredAtUtc=%3")
            .arg(
                recovered->frameId.toString(QUuid::WithoutBraces),
                QString::number(recovered->sequenceNo),
                recovered->recoveredAtUtc.toString(Qt::ISODateWithMs));
        m_operationLogService->write(
            Monitor::Domain::Logs::OperationLogLevel::Info,
            QStringLiteral("Acquisition"),
            QStringLiteral("DataSource.Recovered"),
            QStringLiteral("DataSourceHealthOperationLogConsumer"),
            QStringLiteral("Data source resumed producing frames."),
            detail);
    }
}

bool registerDefaultRuntimeConsumers(
    Monitor::Application::EventBus *eventBus,
    TagCacheConsumer *tagCacheConsumer,
    MeasurementMapFrameConsumer *measurementMapFrameConsumer,
    HistoryRuntimeStateConsumer *historyRuntimeStateConsumer,
    AlarmEventConsumer *alarmEventConsumer,
    AlarmOperationLogConsumer *alarmOperationLogConsumer,
    DataSourceHealthOperationLogConsumer *dataSourceHealthOperationLogConsumer,
    QStringList *errors)
{
    auto success = true;
    success = registerHandler<RawFrameReceivedEvent>(
        eventBus, QStringLiteral("MeasurementMapFrameConsumer"), EventHandlerFailurePolicy::Isolated, 10, measurementMapFrameConsumer, errors) && success;
    success = registerHandler<TagRuntimeStatesProducedEvent>(
        eventBus, QStringLiteral("TagCacheConsumer"), EventHandlerFailurePolicy::Critical, 20, tagCacheConsumer, errors) && success;
    success = registerHandler<DataSourceTimedOutEvent>(
        eventBus, QStringLiteral("DataSourceHealthOperationLogConsumer"), EventHandlerFailurePolicy::Isolated, 30, dataSourceHealthOperationLogConsumer, errors) && success;
    success = registerHandler<DataSourceRecoveredEvent>(
        eventBus, QStringLiteral("DataSourceHealthOperationLogConsumer"), EventHandlerFailurePolicy::Isolated, 40, dataSourceHealthOperationLogConsumer, errors) && success;
    success = registerHandler<TagRuntimeStatesProducedEvent>(
        eventBus, QStringLiteral("HistoryRuntimeStateConsumer"), EventHandlerFailurePolicy::Isolated, 50, historyRuntimeStateConsumer, errors) && success;
    success = registerHandler<AlarmRaisedEvent>(
        eventBus, QStringLiteral("AlarmEventConsumer"), EventHandlerFailurePolicy::Isolated, 60, alarmEventConsumer, errors) && success;
    success = registerHandler<AlarmUpdatedEvent>(
        eventBus, QStringLiteral("AlarmEventConsumer"), EventHandlerFailurePolicy::Isolated, 70, alarmEventConsumer, errors) && success;
    success = registerHandler<AlarmRecoveredEvent>(
        eventBus, QStringLiteral("AlarmEventConsumer"), EventHandlerFailurePolicy::Isolated, 80, alarmEventConsumer, errors) && success;
    success = registerHandler<AlarmAcknowledgedEvent>(
        eventBus, QStringLiteral("AlarmEventConsumer"), EventHandlerFailurePolicy::Isolated, 90, alarmEventConsumer, errors) && success;
    success = registerHandler<AlarmRaisedEvent>(
        eventBus, QStringLiteral("AlarmOperationLogConsumer"), EventHandlerFailurePolicy::Isolated, 100, alarmOperationLogConsumer, errors) && success;
    success = registerHandler<AlarmUpdatedEvent>(
        eventBus, QStringLiteral("AlarmOperationLogConsumer"), EventHandlerFailurePolicy::Isolated, 110, alarmOperationLogConsumer, errors) && success;
    success = registerHandler<AlarmRecoveredEvent>(
        eventBus, QStringLiteral("AlarmOperationLogConsumer"), EventHandlerFailurePolicy::Isolated, 120, alarmOperationLogConsumer, errors) && success;
    success = registerHandler<AlarmAcknowledgedEvent>(
        eventBus, QStringLiteral("AlarmOperationLogConsumer"), EventHandlerFailurePolicy::Isolated, 130, alarmOperationLogConsumer, errors) && success;
    return success;
}

} // namespace Monitor::Application::Services
