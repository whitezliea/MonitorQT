#include "RuntimeLifecycleCoordinator.h"

#include <QMutexLocker>

#include <exception>
#include <stdexcept>
#include <utility>

namespace Monitor::Application::Runtime {

RuntimeLifecycleCoordinator::RuntimeLifecycleCoordinator(RunFunction run)
    : m_run(std::move(run))
{
    if (!m_run) {
        throw std::invalid_argument("Runtime run function must not be empty.");
    }
}

RuntimeLifecycleCoordinator::~RuntimeLifecycleCoordinator()
{
    stop();
}

bool RuntimeLifecycleCoordinator::start()
{
    auto expected = false;
    if (!m_running.compare_exchange_strong(expected, true)) {
        return false;
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }

    m_stopRequested.store(false);
    setStatus({RuntimeLifecycleState::Starting, {}});
    m_thread = std::thread([this]() {
        setStatus({RuntimeLifecycleState::Running, {}});
        try {
            m_run(m_stopRequested);
            if (m_stopRequested.load()) {
                setStatus({RuntimeLifecycleState::Stopped, {}});
            } else {
                setStatus({RuntimeLifecycleState::Stopped, {}});
            }
        } catch (const std::exception &exception) {
            setStatus({RuntimeLifecycleState::Faulted, QString::fromUtf8(exception.what())});
        } catch (...) {
            setStatus({RuntimeLifecycleState::Faulted, QStringLiteral("Unknown runtime failure.")});
        }
        m_running.store(false);
    });
    return true;
}

bool RuntimeLifecycleCoordinator::stop()
{
    if (!m_running.load()) {
        if (m_thread.joinable()) {
            m_thread.join();
        }
        return false;
    }

    setStatus({RuntimeLifecycleState::Stopping, {}});
    m_stopRequested.store(true);
    if (m_thread.joinable()) {
        m_thread.join();
    }

    m_running.store(false);
    if (status().state != RuntimeLifecycleState::Faulted) {
        setStatus({RuntimeLifecycleState::Stopped, {}});
    }
    return true;
}

bool RuntimeLifecycleCoordinator::isActive() const
{
    const auto current = status().state;
    return current == RuntimeLifecycleState::Starting ||
        current == RuntimeLifecycleState::Running ||
        current == RuntimeLifecycleState::Stopping;
}

RuntimeLifecycleStatus RuntimeLifecycleCoordinator::status() const
{
    QMutexLocker locker(&m_mutex);
    return m_status;
}

void RuntimeLifecycleCoordinator::setStatus(const RuntimeLifecycleStatus &status)
{
    QMutexLocker locker(&m_mutex);
    m_status = status;
}

QString toString(RuntimeLifecycleState state)
{
    switch (state) {
    case RuntimeLifecycleState::Stopped:
        return QStringLiteral("Stopped");
    case RuntimeLifecycleState::Starting:
        return QStringLiteral("Starting");
    case RuntimeLifecycleState::Running:
        return QStringLiteral("Running");
    case RuntimeLifecycleState::Stopping:
        return QStringLiteral("Stopping");
    case RuntimeLifecycleState::Faulted:
        return QStringLiteral("Faulted");
    }

    return {};
}

} // namespace Monitor::Application::Runtime
