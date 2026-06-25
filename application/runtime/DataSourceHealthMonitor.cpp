#include "DataSourceHealthMonitor.h"

#include "domain/common/DomainCommon.h"

#include <QMutexLocker>

#include <algorithm>
#include <stdexcept>

namespace Monitor::Application::Runtime {

qint64 DataSourceHealthMonitor::startSession()
{
    QMutexLocker locker(&m_mutex);
    ++m_sessionId;
    m_version = 0;
    m_lastFrameTimer.restart();
    m_lastFrameId = QUuid();
    m_lastSequenceNo = 0;
    m_lastFrameTimeUtc = QDateTime::currentDateTimeUtc();
    m_running = true;
    m_timedOut = false;
    setStatus({DataSourceHealthState::WaitingForFirstFrame, {}, 0, {}});
    return m_sessionId;
}

void DataSourceHealthMonitor::stopSession(qint64 sessionId)
{
    QMutexLocker locker(&m_mutex);
    if (m_sessionId != sessionId) {
        return;
    }

    m_running = false;
    m_timedOut = false;
    ++m_version;
    setStatus({DataSourceHealthState::Stopped, {}, 0, {}});
}

std::optional<Monitor::Application::Events::DataSourceRecoveredEvent> DataSourceHealthMonitor::recordFrame(
    qint64 sessionId,
    const Monitor::Domain::Measurements::RawMeasurementFrame &frame)
{
    Monitor::Domain::Measurements::MeasurementTimeContract::validate(frame);

    QMutexLocker locker(&m_mutex);
    if (!m_running || m_sessionId != sessionId) {
        return std::nullopt;
    }

    const auto recovered = m_timedOut;
    ++m_version;
    m_lastFrameTimer.restart();
    m_lastFrameId = frame.frameId;
    m_lastSequenceNo = frame.sequenceNo;
    m_lastFrameTimeUtc = frame.timestampUtc;
    m_timedOut = false;
    setStatus({DataSourceHealthState::Online, frame.frameId, frame.sequenceNo, frame.timestampUtc});

    if (!recovered) {
        return std::nullopt;
    }

    return Monitor::Application::Events::DataSourceRecoveredEvent{
        frame.frameId,
        frame.sequenceNo,
        QDateTime::currentDateTimeUtc()
    };
}

DataSourceWatchdogSnapshot DataSourceHealthMonitor::snapshot(qint64 sessionId) const
{
    QMutexLocker locker(&m_mutex);
    return {
        sessionId,
        m_version,
        m_running && m_sessionId == sessionId,
        m_timedOut,
        m_lastFrameTimer.isValid() ? m_lastFrameTimer.elapsed() : 0,
        m_lastFrameId,
        m_lastSequenceNo,
        m_lastFrameTimeUtc
    };
}

std::optional<Monitor::Application::Events::DataSourceTimedOutEvent> DataSourceHealthMonitor::tryMarkTimedOut(
    const DataSourceWatchdogSnapshot &expected,
    int timeoutMs)
{
    if (timeoutMs <= 0) {
        throw std::out_of_range("Data source timeout must be greater than zero.");
    }

    QMutexLocker locker(&m_mutex);
    if (!m_running ||
        m_sessionId != expected.sessionId ||
        m_version != expected.version ||
        m_timedOut ||
        m_lastFrameId.isNull() ||
        !m_lastFrameTimer.isValid() ||
        m_lastFrameTimer.elapsed() < timeoutMs) {
        return std::nullopt;
    }

    m_timedOut = true;
    const auto timedOutEvent = Monitor::Application::Events::DataSourceTimedOutEvent{
        m_lastFrameId,
        m_lastSequenceNo,
        Monitor::Domain::Common::UtcDateTime::require(m_lastFrameTimeUtc, QStringLiteral("lastFrameTimeUtc")),
        QDateTime::currentDateTimeUtc()
    };
    setStatus({DataSourceHealthState::TimedOut, m_lastFrameId, m_lastSequenceNo, m_lastFrameTimeUtc});
    return timedOutEvent;
}

int DataSourceHealthMonitor::remainingDelayMs(
    const DataSourceWatchdogSnapshot &snapshot,
    int timeoutMs) const
{
    if (timeoutMs <= 0) {
        throw std::out_of_range("Data source timeout must be greater than zero.");
    }

    return static_cast<int>(std::max<qint64>(0, timeoutMs - snapshot.elapsedSinceLastFrameMs));
}

DataSourceHealthStatus DataSourceHealthMonitor::status() const
{
    QMutexLocker locker(&m_mutex);
    return m_status;
}

void DataSourceHealthMonitor::setStatus(const DataSourceHealthStatus &status)
{
    m_status = status;
}

QString toString(DataSourceHealthState state)
{
    switch (state) {
    case DataSourceHealthState::Stopped:
        return QStringLiteral("Stopped");
    case DataSourceHealthState::WaitingForFirstFrame:
        return QStringLiteral("WaitingForFirstFrame");
    case DataSourceHealthState::Online:
        return QStringLiteral("Online");
    case DataSourceHealthState::TimedOut:
        return QStringLiteral("TimedOut");
    }

    return {};
}

} // namespace Monitor::Application::Runtime
