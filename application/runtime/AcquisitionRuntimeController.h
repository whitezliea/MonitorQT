#ifndef ACQUISITIONRUNTIMECONTROLLER_H
#define ACQUISITIONRUNTIMECONTROLLER_H

#include "application/runtime/PersistenceRuntimeCoordinator.h"
#include "application/runtime/RuntimeLifecycleCoordinator.h"
#include "application/services/OperationLogService.h"

namespace Monitor::Application::Runtime {

class AcquisitionRuntimeController
{
public:
    AcquisitionRuntimeController(
        RuntimeLifecycleCoordinator *runtimeLifecycle,
        PersistenceRuntimeCoordinator *persistenceRuntime,
        Monitor::Application::Services::OperationLogService *operationLogService);

    bool start();
    bool stop();

private:
    RuntimeLifecycleCoordinator *m_runtimeLifecycle = nullptr;
    PersistenceRuntimeCoordinator *m_persistenceRuntime = nullptr;
    Monitor::Application::Services::OperationLogService *m_operationLogService = nullptr;
};

} // namespace Monitor::Application::Runtime

#endif // ACQUISITIONRUNTIMECONTROLLER_H
