#include "AcquisitionRuntimeController.h"

#include <optional>
#include <stdexcept>

namespace Monitor::Application::Runtime {

AcquisitionRuntimeController::AcquisitionRuntimeController(
    RuntimeLifecycleCoordinator *runtimeLifecycle,
    PersistenceRuntimeCoordinator *persistenceRuntime,
    Monitor::Application::Services::OperationLogService *operationLogService)
    : m_runtimeLifecycle(runtimeLifecycle),
      m_persistenceRuntime(persistenceRuntime),
      m_operationLogService(operationLogService)
{
    if (!m_runtimeLifecycle || !m_persistenceRuntime || !m_operationLogService) {
        throw std::invalid_argument("AcquisitionRuntimeController dependencies must not be null.");
    }
}

bool AcquisitionRuntimeController::start()
{
    if (!m_runtimeLifecycle->start()) {
        return false;
    }

    m_operationLogService->write(
        Monitor::Domain::Logs::OperationLogLevel::Info,
        QStringLiteral("Acquisition"),
        QStringLiteral("Acquisition.Started"),
        QStringLiteral("AcquisitionRuntimeController"),
        QStringLiteral("Acquisition started."));
    return true;
}

bool AcquisitionRuntimeController::stop()
{
    if (!m_runtimeLifecycle->stop()) {
        return false;
    }

    QString detail;
    try {
        m_persistenceRuntime->flushHistory();
    } catch (const std::exception &exception) {
        detail = QStringLiteral("HistoryFlushError=%1").arg(QString::fromUtf8(exception.what()));
        m_operationLogService->write(
            Monitor::Domain::Logs::OperationLogLevel::Error,
            QStringLiteral("Persistence"),
            QStringLiteral("History.FlushFailed"),
            QStringLiteral("AcquisitionRuntimeController"),
            QStringLiteral("History flush failed after acquisition stopped."),
            detail);
    }

    m_operationLogService->write(
        Monitor::Domain::Logs::OperationLogLevel::Info,
        QStringLiteral("Acquisition"),
        QStringLiteral("Acquisition.Stopped"),
        QStringLiteral("AcquisitionRuntimeController"),
        QStringLiteral("Acquisition stopped."),
        detail.isEmpty() ? std::optional<QString>() : std::optional<QString>(detail));
    return true;
}

} // namespace Monitor::Application::Runtime
