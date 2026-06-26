#include "RuntimeCommandFacade.h"

#include "application/configuration/ConfigurationValidation.h"
#include "domain/logs/LogModels.h"

#include <QDateTime>

#include <exception>
#include <stdexcept>

namespace Monitor::Application::Services {

RuntimeCommandFacade::RuntimeCommandFacade(
    Monitor::Application::Runtime::AcquisitionRuntimeController *acquisitionController,
    Monitor::Application::Runtime::MonitoringRuntimeService *monitoringRuntimeService,
    Monitor::Application::Configuration::RuntimeOptionsStore *runtimeOptionsStore,
    Monitor::Application::Configuration::TagRuntimeConfigurationStore *tagRuntimeConfigurationStore,
    AlarmService *alarmService,
    OperationLogService *operationLogService)
    : m_acquisitionController(acquisitionController),
      m_monitoringRuntimeService(monitoringRuntimeService),
      m_runtimeOptionsStore(runtimeOptionsStore),
      m_tagRuntimeConfigurationStore(tagRuntimeConfigurationStore),
      m_alarmService(alarmService),
      m_operationLogService(operationLogService)
{
    if (!m_acquisitionController ||
        !m_monitoringRuntimeService ||
        !m_runtimeOptionsStore ||
        !m_tagRuntimeConfigurationStore ||
        !m_alarmService ||
        !m_operationLogService) {
        throw std::invalid_argument("RuntimeCommandFacade dependencies must not be null.");
    }
}

bool RuntimeCommandFacade::start(QStringList *errors)
{
    if (!m_acquisitionController->start()) {
        appendError(errors, QStringLiteral("Acquisition runtime is already running or could not be started."));
        return false;
    }

    return true;
}

bool RuntimeCommandFacade::stop(QStringList *errors)
{
    if (!m_acquisitionController->stop()) {
        appendError(errors, QStringLiteral("Acquisition runtime is already stopped or could not be stopped."));
        return false;
    }

    return true;
}

bool RuntimeCommandFacade::acknowledgeAlarm(const QUuid &alarmId, QStringList *errors)
{
    Monitor::Domain::Alarms::AlarmEvent acknowledgedAlarm;
    QStringList localErrors;
    const auto success = m_monitoringRuntimeService->acknowledgeAlarm(
        alarmId,
        QDateTime::currentDateTimeUtc(),
        &acknowledgedAlarm,
        &localErrors);
    if (!success) {
        if (errors) {
            errors->append(localErrors);
        }
        return false;
    }

    return true;
}

bool RuntimeCommandFacade::saveRuntimeOptions(
    const Monitor::Application::Configuration::MonitorRuntimeOptions &options,
    QStringList *errors)
{
    try {
        Monitor::Application::Configuration::ConfigurationValidation::validateRuntimeOptions(options);
        m_runtimeOptionsStore->replace(options);
        m_operationLogService->write(
            Monitor::Domain::Logs::OperationLogLevel::Info,
            QStringLiteral("Settings"),
            QStringLiteral("RuntimeOptions.Save"),
            QStringLiteral("RuntimeCommandFacade"),
            QStringLiteral("Runtime options updated in memory."),
            QStringLiteral("UiRefreshIntervalMs=%1; DataGenerateIntervalMs=%2; HistoryRetentionDays=%3")
                .arg(options.uiRefreshIntervalMs)
                .arg(options.dataGenerateIntervalMs)
                .arg(options.historyRetentionDays));
        return true;
    } catch (const std::exception &exception) {
        appendError(errors, QStringLiteral("Runtime options save failed: %1").arg(QString::fromUtf8(exception.what())));
        return false;
    }
}

bool RuntimeCommandFacade::saveTagConfigurations(
    const QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> &configurations,
    QStringList *errors)
{
    if (configurations.isEmpty()) {
        appendError(errors, QStringLiteral("Tag configurations cannot be empty."));
        return false;
    }

    try {
        m_tagRuntimeConfigurationStore->replace(configurations);
        m_alarmService->replaceConfigurations(configurations);
        m_operationLogService->write(
            Monitor::Domain::Logs::OperationLogLevel::Info,
            QStringLiteral("Settings"),
            QStringLiteral("TagConfigurations.Save"),
            QStringLiteral("RuntimeCommandFacade"),
            QStringLiteral("Tag runtime configurations updated in memory."),
            QStringLiteral("Count=%1").arg(configurations.size()));
        return true;
    } catch (const std::exception &exception) {
        appendError(errors, QStringLiteral("Tag configurations save failed: %1").arg(QString::fromUtf8(exception.what())));
        return false;
    }
}

void RuntimeCommandFacade::appendError(QStringList *errors, const QString &message) const
{
    if (errors) {
        errors->append(message);
    }
}

} // namespace Monitor::Application::Services
