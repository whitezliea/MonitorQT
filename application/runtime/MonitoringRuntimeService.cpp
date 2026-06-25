#include "MonitoringRuntimeService.h"

#include "domain/common/DomainCommon.h"

#include <QThread>

#include <stdexcept>

namespace Monitor::Application::Runtime {
namespace {

void appendErrors(QStringList *target, const QStringList &source)
{
    if (target) {
        target->append(source);
    }
}

} // namespace

MonitoringRuntimeService::MonitoringRuntimeService(
    Monitor::Application::Abstractions::IRawFrameSource *dataSource,
    Monitor::Application::Pipelines::DataCleanPipeline *dataCleanPipeline,
    Monitor::Application::Services::AlarmService *alarmService,
    Monitor::Application::EventBus *eventBus,
    const Monitor::Application::Configuration::MonitorRuntimeOptions &runtimeOptions,
    DataSourceHealthMonitor *healthMonitor)
    : m_dataSource(dataSource),
      m_dataCleanPipeline(dataCleanPipeline),
      m_alarmService(alarmService),
      m_eventBus(eventBus),
      m_runtimeOptions(runtimeOptions),
      m_healthMonitor(healthMonitor ? healthMonitor : &m_ownedHealthMonitor)
{
    if (!m_dataSource || !m_dataCleanPipeline || !m_alarmService || !m_eventBus || !m_healthMonitor) {
        throw std::invalid_argument("MonitoringRuntimeService dependencies must not be null.");
    }
}

void MonitoringRuntimeService::run(std::atomic_bool &stopRequested)
{
    m_dataCleanPipeline->resetSession();
    m_dataSource->resetCancellation();
    m_sessionId = m_healthMonitor->startSession();
    m_lastCleanedValues.clear();

    try {
        while (!stopRequested.load()) {
            Monitor::Domain::Measurements::RawMeasurementFrame frame;
            const auto timestamp = QDateTime::currentDateTimeUtc();
            if (m_dataSource->readNextFrame(timestamp, &frame)) {
                QStringList errors;
                if (!processFrame(frame, &errors)) {
                    throw std::runtime_error(errors.join(QStringLiteral("; ")).toStdString());
                }
            } else {
                QStringList errors;
                if (!publishOfflineStatesIfTimedOut(QDateTime::currentDateTimeUtc(), &errors)) {
                    throw std::runtime_error(errors.join(QStringLiteral("; ")).toStdString());
                }
            }

            const auto sleepMs = qMax(1, m_runtimeOptions.dataGenerateIntervalMs);
            auto elapsed = 0;
            while (elapsed < sleepMs && !stopRequested.load()) {
                const auto chunk = qMin(25, sleepMs - elapsed);
                QThread::msleep(static_cast<unsigned long>(chunk));
                elapsed += chunk;
            }
        }
    } catch (...) {
        m_dataSource->cancel();
        m_healthMonitor->stopSession(m_sessionId);
        throw;
    }

    m_dataSource->cancel();
    m_healthMonitor->stopSession(m_sessionId);
}

bool MonitoringRuntimeService::processFrame(
    const Monitor::Domain::Measurements::RawMeasurementFrame &frame,
    QStringList *errors)
{
    Monitor::Domain::Measurements::MeasurementTimeContract::validate(frame);
    if (m_sessionId == 0 || m_healthMonitor->status().state == DataSourceHealthState::Stopped) {
        m_sessionId = m_healthMonitor->startSession();
    }

    auto recoveredEvent = m_healthMonitor->recordFrame(m_sessionId, frame);
    if (!m_eventBus->publish(Monitor::Application::Events::RawFrameReceivedEvent{frame}, errors)) {
        return false;
    }
    if (recoveredEvent.has_value() &&
        !m_eventBus->publish(recoveredEvent.value(), errors)) {
        return false;
    }

    const auto cleanedValues = m_dataCleanPipeline->cleanToCleanedValues(frame);
    if (!publishEvaluatedStates(frame.frameId, frame.sequenceNo, frame.timestampUtc, cleanedValues, errors)) {
        return false;
    }

    m_lastCleanedValues = cleanedValues;
    return true;
}

bool MonitoringRuntimeService::publishOfflineStatesIfTimedOut(
    const QDateTime &,
    QStringList *errors)
{
    if (m_lastCleanedValues.isEmpty()) {
        return true;
    }

    const auto watchdogSnapshot = m_healthMonitor->snapshot(m_sessionId);
    const auto timedOutEvent = m_healthMonitor->tryMarkTimedOut(
        watchdogSnapshot,
        m_runtimeOptions.dataSourceTimeoutMs());
    if (!timedOutEvent.has_value()) {
        return true;
    }

    if (!m_eventBus->publish(timedOutEvent.value(), errors)) {
        return false;
    }

    const auto offlineValues = createOfflineValues(timedOutEvent.value());
    return publishEvaluatedStates(
        QUuid::createUuid(),
        timedOutEvent->lastSequenceNo,
        timedOutEvent->timedOutAtUtc,
        offlineValues,
        errors);
}

DataSourceHealthMonitor &MonitoringRuntimeService::healthMonitor()
{
    return *m_healthMonitor;
}

bool MonitoringRuntimeService::publishEvaluatedStates(
    const QUuid &frameId,
    qint64 sequenceNo,
    const QDateTime &timestampUtc,
    const QVector<Monitor::Domain::Tags::CleanedTagValue> &cleanedValues,
    QStringList *errors)
{
    const auto evaluation = m_alarmService->evaluateWithChanges(
        cleanedValues,
        QDateTime::currentDateTimeUtc());
    if (!m_eventBus->publish(
            Monitor::Application::Events::TagRuntimeStatesProducedEvent{
                frameId,
                sequenceNo,
                timestampUtc,
                evaluation.states
            },
            errors)) {
        return false;
    }

    for (const auto &change : evaluation.lifecycleChanges) {
        if (!publishAlarmChange(change, errors)) {
            return false;
        }
    }

    return true;
}

bool MonitoringRuntimeService::publishAlarmChange(
    const Monitor::Application::Dtos::AlarmLifecycleChange &change,
    QStringList *errors)
{
    using Monitor::Application::Dtos::AlarmLifecycleChangeType;

    switch (change.changeType) {
    case AlarmLifecycleChangeType::Raised:
        return m_eventBus->publish(Monitor::Application::Events::AlarmRaisedEvent{change.alarm}, errors);
    case AlarmLifecycleChangeType::Updated:
        return m_eventBus->publish(Monitor::Application::Events::AlarmUpdatedEvent{change.alarm}, errors);
    case AlarmLifecycleChangeType::Recovered:
        return m_eventBus->publish(Monitor::Application::Events::AlarmRecoveredEvent{change.alarm}, errors);
    }

    if (errors) {
        errors->append(QStringLiteral("Unsupported alarm lifecycle change."));
    }
    return false;
}

QVector<Monitor::Domain::Tags::CleanedTagValue> MonitoringRuntimeService::createOfflineValues(
    const Monitor::Application::Events::DataSourceTimedOutEvent &timedOutEvent) const
{
    QVector<Monitor::Domain::Tags::CleanedTagValue> offlineValues;
    offlineValues.reserve(m_lastCleanedValues.size());
    const auto timeoutTransitionId = QUuid::createUuid();
    for (auto value : m_lastCleanedValues) {
        value.timestampUtc = timedOutEvent.timedOutAtUtc;
        value.quality = Monitor::Domain::Tags::TagQuality::Offline;
        value.sourceFrameId = timeoutTransitionId;
        value.sequenceNo = timedOutEvent.lastSequenceNo;
        value.cleanMessage = QStringLiteral("Data source timed out; last value retained.");
        offlineValues.append(value);
    }

    return offlineValues;
}

} // namespace Monitor::Application::Runtime
