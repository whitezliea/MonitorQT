#include "PersistenceRuntimeCoordinator.h"

#include <algorithm>
#include <stdexcept>

namespace Monitor::Application::Runtime {

PersistenceRuntimeCoordinator::PersistenceRuntimeCoordinator(
    const QVector<Monitor::Application::Workers::IPersistWorker *> &workers)
    : m_workers(workers)
{
    if (m_workers.isEmpty()) {
        throw std::invalid_argument("At least one persistence worker is required.");
    }

    if (std::any_of(m_workers.cbegin(), m_workers.cend(), [](const auto *worker) {
            return worker == nullptr;
        })) {
        throw std::invalid_argument("Persistence workers must not contain null entries.");
    }
}

PersistenceRuntimeCoordinator::~PersistenceRuntimeCoordinator()
{
    stop();
}

bool PersistenceRuntimeCoordinator::start()
{
    auto startedAny = false;
    for (auto *worker : m_workers) {
        startedAny = worker->start() || startedAny;
    }

    refreshStatus();
    return startedAny;
}

bool PersistenceRuntimeCoordinator::stop()
{
    auto stoppedAny = false;
    for (auto *worker : m_workers) {
        stoppedAny = worker->stop() || stoppedAny;
    }

    refreshStatus();
    if (!isRunning()) {
        setStatus({PersistenceRuntimeState::Stopped, {}, {}});
    }
    return stoppedAny;
}

bool PersistenceRuntimeCoordinator::flushWorker(const QString &workerName)
{
    const auto it = std::find_if(m_workers.begin(), m_workers.end(), [&workerName](const auto *worker) {
        return worker->name() == workerName;
    });
    if (it == m_workers.end()) {
        throw std::invalid_argument(QStringLiteral("Unknown persistence worker: %1").arg(workerName).toStdString());
    }

    const auto result = (*it)->flush();
    refreshStatus();
    return result;
}

bool PersistenceRuntimeCoordinator::flushHistory()
{
    return flushWorker(QStringLiteral("History"));
}

bool PersistenceRuntimeCoordinator::flushAlarms()
{
    return flushWorker(QStringLiteral("Alarm"));
}

bool PersistenceRuntimeCoordinator::flushOperationLogs()
{
    return flushWorker(QStringLiteral("OperationLog"));
}

bool PersistenceRuntimeCoordinator::isRunning() const
{
    return m_status.state == PersistenceRuntimeState::Running ||
        m_status.state == PersistenceRuntimeState::Degraded;
}

PersistenceRuntimeStatus PersistenceRuntimeCoordinator::status() const
{
    return m_status;
}

void PersistenceRuntimeCoordinator::refreshStatus()
{
    for (const auto *worker : m_workers) {
        const auto workerStatus = worker->status();
        if (workerStatus.state == Monitor::Application::Workers::PersistWorkerState::Faulted) {
            setStatus({PersistenceRuntimeState::Faulted, worker->name(), workerStatus.lastError});
            return;
        }
    }

    for (const auto *worker : m_workers) {
        const auto workerStatus = worker->status();
        if (workerStatus.state == Monitor::Application::Workers::PersistWorkerState::Degraded) {
            setStatus({PersistenceRuntimeState::Degraded, worker->name(), workerStatus.lastError});
            return;
        }
    }

    const auto allRunning = std::all_of(m_workers.cbegin(), m_workers.cend(), [](const auto *worker) {
        return worker->isRunning();
    });
    setStatus({allRunning ? PersistenceRuntimeState::Running : PersistenceRuntimeState::Stopped, {}, {}});
}

void PersistenceRuntimeCoordinator::setStatus(const PersistenceRuntimeStatus &status)
{
    m_status = status;
}

QString toString(PersistenceRuntimeState state)
{
    switch (state) {
    case PersistenceRuntimeState::Stopped:
        return QStringLiteral("Stopped");
    case PersistenceRuntimeState::Running:
        return QStringLiteral("Running");
    case PersistenceRuntimeState::Degraded:
        return QStringLiteral("Degraded");
    case PersistenceRuntimeState::Faulted:
        return QStringLiteral("Faulted");
    }

    return {};
}

} // namespace Monitor::Application::Runtime
