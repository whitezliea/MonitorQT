#include "ApplicationRuntimeHost.h"

#include "application/runtime/PersistenceRuntimeCoordinator.h"
#include "application/runtime/RuntimeLifecycleCoordinator.h"
#include "application/services/OperationLogService.h"
#include "domain/logs/LogModels.h"
#include "infrastructure/persistence/HistoryRetentionService.h"

#include <QDateTime>
#include <QVector>

#include <exception>
#include <stdexcept>

namespace Monitor::Bootstrap {

ApplicationRuntimeHost::ApplicationRuntimeHost(
    Monitor::Application::Runtime::RuntimeLifecycleCoordinator *runtimeLifecycle,
    Monitor::Application::Runtime::PersistenceRuntimeCoordinator *persistenceRuntime,
    Monitor::Infrastructure::Persistence::HistoryRetentionService *historyRetentionService,
    Monitor::Application::Services::OperationLogService *operationLogService)
    : m_runtimeLifecycle(runtimeLifecycle),
      m_persistenceRuntime(persistenceRuntime),
      m_historyRetentionService(historyRetentionService),
      m_operationLogService(operationLogService)
{
    if (!m_runtimeLifecycle || !m_persistenceRuntime || !m_historyRetentionService || !m_operationLogService) {
        throw std::invalid_argument("ApplicationRuntimeHost dependencies must not be null.");
    }
}

ApplicationRuntimeHost::~ApplicationRuntimeHost()
{
    stop();
}

bool ApplicationRuntimeHost::start(QStringList *errors)
{
    if (m_started) {
        return true;
    }

    try {
        m_persistenceRuntime->start();
    } catch (const std::exception &exception) {
        appendError(errors, QStringLiteral("Persistence runtime start failed: %1").arg(QString::fromUtf8(exception.what())));
        return false;
    }

    m_started = m_persistenceRuntime->isRunning();
    if (!m_started) {
        appendError(errors, QStringLiteral("Persistence runtime did not enter a running state."));
        return false;
    }

    try {
        const auto retention = m_historyRetentionService->cleanup(QDateTime::currentDateTimeUtc());
        m_operationLogService->write(
            Monitor::Domain::Logs::OperationLogLevel::Info,
            QStringLiteral("Application"),
            QStringLiteral("ApplicationRuntimeHost.Started"),
            QStringLiteral("ApplicationRuntimeHost"),
            QStringLiteral("Application runtime host started."),
            QStringLiteral("HistoryRetentionDeleted=%1; CutoffUtc=%2")
                .arg(retention.deletedCount)
                .arg(retention.cutoffUtc.toString(Qt::ISODateWithMs)));
    } catch (const std::exception &exception) {
        const auto message = QStringLiteral("History retention cleanup failed: %1").arg(QString::fromUtf8(exception.what()));
        appendError(errors, message);
        try {
            m_operationLogService->write(
                Monitor::Domain::Logs::OperationLogLevel::Error,
                QStringLiteral("Application"),
                QStringLiteral("ApplicationRuntimeHost.RetentionFailed"),
                QStringLiteral("ApplicationRuntimeHost"),
                message);
        } catch (...) {
        }
        return false;
    }

    return true;
}

bool ApplicationRuntimeHost::stop(QStringList *errors)
{
    auto success = true;

    if (m_runtimeLifecycle->isActive()) {
        if (!m_runtimeLifecycle->stop()) {
            appendError(errors, QStringLiteral("Acquisition runtime stop returned false while runtime was active."));
            success = false;
        }
    }

    if (m_started || m_persistenceRuntime->isRunning()) {
        try {
            m_operationLogService->write(
                Monitor::Domain::Logs::OperationLogLevel::Info,
                QStringLiteral("Application"),
                QStringLiteral("ApplicationRuntimeHost.Stopping"),
                QStringLiteral("ApplicationRuntimeHost"),
                QStringLiteral("Application runtime host is stopping."));
        } catch (const std::exception &exception) {
            appendError(errors, QStringLiteral("Application stop log enqueue failed: %1").arg(QString::fromUtf8(exception.what())));
            success = false;
        }

        success = flushAll(errors) && success;

        try {
            m_persistenceRuntime->stop();
        } catch (const std::exception &exception) {
            appendError(errors, QStringLiteral("Persistence runtime stop failed: %1").arg(QString::fromUtf8(exception.what())));
            success = false;
        }
    }

    m_started = false;
    return success;
}

bool ApplicationRuntimeHost::isStarted() const
{
    return m_started;
}

void ApplicationRuntimeHost::appendError(QStringList *errors, const QString &message) const
{
    if (errors) {
        errors->append(message);
    }
}

bool ApplicationRuntimeHost::flushAll(QStringList *errors)
{
    auto success = true;
    const QVector<QString> workerNames = {
        QStringLiteral("History"),
        QStringLiteral("Alarm"),
        QStringLiteral("OperationLog")
    };

    for (const auto &workerName : workerNames) {
        try {
            bool flushed = false;
            if (workerName == QStringLiteral("History")) {
                flushed = m_persistenceRuntime->flushHistory();
            } else if (workerName == QStringLiteral("Alarm")) {
                flushed = m_persistenceRuntime->flushAlarms();
            } else if (workerName == QStringLiteral("OperationLog")) {
                flushed = m_persistenceRuntime->flushOperationLogs();
            } else {
                flushed = m_persistenceRuntime->flushWorker(workerName);
            }

            if (!flushed) {
                appendError(errors, QStringLiteral("%1 persistence worker flush reported failure.").arg(workerName));
                success = false;
            }
        } catch (const std::exception &exception) {
            appendError(
                errors,
                QStringLiteral("%1 persistence worker flush failed: %2")
                    .arg(workerName, QString::fromUtf8(exception.what())));
            success = false;
        }
    }

    return success;
}

} // namespace Monitor::Bootstrap
