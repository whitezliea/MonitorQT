#ifndef DATASOURCEHEALTHMONITOR_H
#define DATASOURCEHEALTHMONITOR_H

#include "application/events/ApplicationEvents.h"
#include "domain/measurements/MeasurementModels.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QMutex>
#include <QUuid>

#include <optional>

namespace Monitor::Application::Runtime {

enum class DataSourceHealthState {
    Stopped,
    WaitingForFirstFrame,
    Online,
    TimedOut
};

struct DataSourceHealthStatus
{
    DataSourceHealthState state = DataSourceHealthState::Stopped;
    QUuid lastFrameId;
    qint64 lastSequenceNo = 0;
    QDateTime lastFrameTimeUtc;
};

struct DataSourceWatchdogSnapshot
{
    qint64 sessionId = 0;
    qint64 version = 0;
    bool isRunning = false;
    bool isTimedOut = false;
    qint64 elapsedSinceLastFrameMs = 0;
    QUuid lastFrameId;
    qint64 lastSequenceNo = 0;
    QDateTime lastFrameTimeUtc;
};

class DataSourceHealthMonitor
{
public:
    qint64 startSession();
    void stopSession(qint64 sessionId);
    std::optional<Monitor::Application::Events::DataSourceRecoveredEvent> recordFrame(
        qint64 sessionId,
        const Monitor::Domain::Measurements::RawMeasurementFrame &frame);
    DataSourceWatchdogSnapshot snapshot(qint64 sessionId) const;
    std::optional<Monitor::Application::Events::DataSourceTimedOutEvent> tryMarkTimedOut(
        const DataSourceWatchdogSnapshot &expected,
        int timeoutMs);
    int remainingDelayMs(const DataSourceWatchdogSnapshot &snapshot, int timeoutMs) const;
    DataSourceHealthStatus status() const;

private:
    void setStatus(const DataSourceHealthStatus &status);

    mutable QMutex m_mutex;
    qint64 m_sessionId = 0;
    qint64 m_version = 0;
    QElapsedTimer m_lastFrameTimer;
    QUuid m_lastFrameId;
    qint64 m_lastSequenceNo = 0;
    QDateTime m_lastFrameTimeUtc;
    bool m_running = false;
    bool m_timedOut = false;
    DataSourceHealthStatus m_status;
};

QString toString(DataSourceHealthState state);

} // namespace Monitor::Application::Runtime

#endif // DATASOURCEHEALTHMONITOR_H
