#ifndef RUNTIMELIFECYCLECOORDINATOR_H
#define RUNTIMELIFECYCLECOORDINATOR_H

#include <QMutex>
#include <QString>

#include <atomic>
#include <functional>
#include <thread>

namespace Monitor::Application::Runtime {

enum class RuntimeLifecycleState {
    Stopped,
    Starting,
    Running,
    Stopping,
    Faulted
};

struct RuntimeLifecycleStatus
{
    RuntimeLifecycleState state = RuntimeLifecycleState::Stopped;
    QString errorMessage;
};

class RuntimeLifecycleCoordinator
{
public:
    using RunFunction = std::function<void(std::atomic_bool &)>;

    explicit RuntimeLifecycleCoordinator(RunFunction run);
    ~RuntimeLifecycleCoordinator();

    RuntimeLifecycleCoordinator(const RuntimeLifecycleCoordinator &) = delete;
    RuntimeLifecycleCoordinator &operator=(const RuntimeLifecycleCoordinator &) = delete;

    bool start();
    bool stop();
    bool isActive() const;
    RuntimeLifecycleStatus status() const;

private:
    void setStatus(const RuntimeLifecycleStatus &status);

    RunFunction m_run;
    mutable QMutex m_mutex;
    RuntimeLifecycleStatus m_status;
    std::thread m_thread;
    std::atomic_bool m_stopRequested = false;
    std::atomic_bool m_running = false;
};

QString toString(RuntimeLifecycleState state);

} // namespace Monitor::Application::Runtime

#endif // RUNTIMELIFECYCLECOORDINATOR_H
