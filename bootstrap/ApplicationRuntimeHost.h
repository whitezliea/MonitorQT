#ifndef APPLICATIONRUNTIMEHOST_H
#define APPLICATIONRUNTIMEHOST_H

#include <QStringList>

namespace Monitor::Application::Runtime {

class PersistenceRuntimeCoordinator;
class RuntimeLifecycleCoordinator;

} // namespace Monitor::Application::Runtime

namespace Monitor::Application::Services {

class OperationLogService;

} // namespace Monitor::Application::Services

namespace Monitor::Infrastructure::Persistence {

class HistoryRetentionService;

} // namespace Monitor::Infrastructure::Persistence

namespace Monitor::Bootstrap {

class ApplicationRuntimeHost
{
public:
    ApplicationRuntimeHost(
        Monitor::Application::Runtime::RuntimeLifecycleCoordinator *runtimeLifecycle,
        Monitor::Application::Runtime::PersistenceRuntimeCoordinator *persistenceRuntime,
        Monitor::Infrastructure::Persistence::HistoryRetentionService *historyRetentionService,
        Monitor::Application::Services::OperationLogService *operationLogService);
    ~ApplicationRuntimeHost();

    ApplicationRuntimeHost(const ApplicationRuntimeHost &) = delete;
    ApplicationRuntimeHost &operator=(const ApplicationRuntimeHost &) = delete;

    bool start(QStringList *errors = nullptr);
    bool stop(QStringList *errors = nullptr);
    bool isStarted() const;

private:
    void appendError(QStringList *errors, const QString &message) const;
    bool flushAll(QStringList *errors);

    Monitor::Application::Runtime::RuntimeLifecycleCoordinator *m_runtimeLifecycle = nullptr;
    Monitor::Application::Runtime::PersistenceRuntimeCoordinator *m_persistenceRuntime = nullptr;
    Monitor::Infrastructure::Persistence::HistoryRetentionService *m_historyRetentionService = nullptr;
    Monitor::Application::Services::OperationLogService *m_operationLogService = nullptr;
    bool m_started = false;
};

} // namespace Monitor::Bootstrap

#endif // APPLICATIONRUNTIMEHOST_H
