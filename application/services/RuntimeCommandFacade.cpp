#include "RuntimeCommandFacade.h"

#include "application/configuration/ConfigurationValidation.h"
#include "application/services/RuntimeEventConsumers.h"
#include "domain/logs/LogModels.h"

#include <QDateTime>

#include <exception>
#include <stdexcept>
#include <utility>

namespace Monitor::Application::Services {
namespace {

QHash<QString, QString> runtimeSettingsMap(
    const Monitor::Application::Configuration::MonitorRuntimeOptions &options)
{
    using Monitor::Application::Configuration::RuntimeSettingKeys;

    return {
        {RuntimeSettingKeys::DataGenerateIntervalMs, QString::number(options.dataGenerateIntervalMs)},
        {RuntimeSettingKeys::DataSourceTimeoutPeriods, QString::number(options.dataSourceTimeoutPeriods)},
        {RuntimeSettingKeys::UiRefreshIntervalMs, QString::number(options.uiRefreshIntervalMs)},
        {RuntimeSettingKeys::HistoryBatchIntervalMs, QString::number(options.historyBatchIntervalMs)},
        {RuntimeSettingKeys::HistoryRetentionDays, QString::number(options.historyRetentionDays)},
        {RuntimeSettingKeys::AlarmBatchIntervalMs, QString::number(options.alarmBatchIntervalMs)},
        {RuntimeSettingKeys::OperationLogBatchIntervalMs, QString::number(options.operationLogBatchIntervalMs)},
        {RuntimeSettingKeys::MaximumTrendWindowMinutes, QString::number(options.maximumTrendWindowMinutes())}
    };
}

QString runtimeOptionsEffectDetail(
    const Monitor::Application::Configuration::MonitorRuntimeOptions &options)
{
    using Monitor::Application::Configuration::SettingEffect;
    using Monitor::Application::Configuration::toString;

    return QStringLiteral(
               "UiRefreshIntervalMs=%1 (%2); DataGenerateIntervalMs=%3 (%4); "
               "DataSourceTimeoutPeriods=%5 (%6); HistoryBatchIntervalMs=%7 (%8); "
               "AlarmBatchIntervalMs=%9 (%10); OperationLogBatchIntervalMs=%11 (%12); "
               "HistoryRetentionDays=%13 (%14); MaximumTrendWindowMinutes=%15 (%16)")
        .arg(options.uiRefreshIntervalMs)
        .arg(toString(SettingEffect::Immediate))
        .arg(options.dataGenerateIntervalMs)
        .arg(toString(SettingEffect::NextAcquisitionStart))
        .arg(options.dataSourceTimeoutPeriods)
        .arg(toString(SettingEffect::NextAcquisitionStart))
        .arg(options.historyBatchIntervalMs)
        .arg(toString(SettingEffect::NextApplicationStart))
        .arg(options.alarmBatchIntervalMs)
        .arg(toString(SettingEffect::NextApplicationStart))
        .arg(options.operationLogBatchIntervalMs)
        .arg(toString(SettingEffect::NextApplicationStart))
        .arg(options.historyRetentionDays)
        .arg(toString(SettingEffect::NextApplicationStart))
        .arg(options.maximumTrendWindowMinutes())
        .arg(toString(SettingEffect::NextApplicationStart));
}

} // namespace

RuntimeCommandFacade::RuntimeCommandFacade(
    Monitor::Application::Runtime::AcquisitionRuntimeController *acquisitionController,
    Monitor::Application::Runtime::MonitoringRuntimeService *monitoringRuntimeService,
    Monitor::Application::Configuration::RuntimeOptionsStore *runtimeOptionsStore,
    Monitor::Application::Configuration::TagRuntimeConfigurationStore *tagRuntimeConfigurationStore,
    AlarmService *alarmService,
    HistoryRuntimeStateConsumer *historyRuntimeStateConsumer,
    OperationLogService *operationLogService)
    : RuntimeCommandFacade(
          acquisitionController,
          monitoringRuntimeService,
          runtimeOptionsStore,
          tagRuntimeConfigurationStore,
          alarmService,
          historyRuntimeStateConsumer,
          operationLogService,
          {},
          [](const QHash<QString, QString> &) {},
          [](const QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> &) {})
{
}

RuntimeCommandFacade::RuntimeCommandFacade(
    Monitor::Application::Runtime::AcquisitionRuntimeController *acquisitionController,
    Monitor::Application::Runtime::MonitoringRuntimeService *monitoringRuntimeService,
    Monitor::Application::Configuration::RuntimeOptionsStore *runtimeOptionsStore,
    Monitor::Application::Configuration::TagRuntimeConfigurationStore *tagRuntimeConfigurationStore,
    AlarmService *alarmService,
    HistoryRuntimeStateConsumer *historyRuntimeStateConsumer,
    OperationLogService *operationLogService,
    QVector<Monitor::Domain::Tags::TagDefinition> tagDefinitions,
    SaveRuntimeSettingsFunction saveRuntimeSettings,
    SaveTagConfigurationsFunction saveTagConfigurations)
    : m_acquisitionController(acquisitionController),
      m_monitoringRuntimeService(monitoringRuntimeService),
      m_runtimeOptionsStore(runtimeOptionsStore),
      m_tagRuntimeConfigurationStore(tagRuntimeConfigurationStore),
      m_alarmService(alarmService),
      m_historyRuntimeStateConsumer(historyRuntimeStateConsumer),
      m_operationLogService(operationLogService),
      m_tagDefinitions(std::move(tagDefinitions)),
      m_saveRuntimeSettings(std::move(saveRuntimeSettings)),
      m_saveTagConfigurations(std::move(saveTagConfigurations))
{
    if (!m_acquisitionController ||
        !m_monitoringRuntimeService ||
        !m_runtimeOptionsStore ||
        !m_tagRuntimeConfigurationStore ||
        !m_alarmService ||
        !m_historyRuntimeStateConsumer ||
        !m_operationLogService ||
        !m_saveRuntimeSettings ||
        !m_saveTagConfigurations) {
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
        m_saveRuntimeSettings(runtimeSettingsMap(options));
        m_runtimeOptionsStore->replace(options);
        m_operationLogService->write(
            Monitor::Domain::Logs::OperationLogLevel::Info,
            QStringLiteral("Settings"),
            QStringLiteral("RuntimeOptions.Save"),
            QStringLiteral("RuntimeCommandFacade"),
            QStringLiteral("Runtime options saved and synchronized."),
            runtimeOptionsEffectDetail(options));
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
        if (!m_tagDefinitions.isEmpty()) {
            QHash<QString, Monitor::Domain::Tags::TagDefinition> definitionsByTagId;
            for (const auto &definition : m_tagDefinitions) {
                definitionsByTagId.insert(definition.tagId, definition);
            }
            for (const auto &configuration : configurations) {
                const auto definitionIt = definitionsByTagId.constFind(configuration.tagId);
                if (definitionIt == definitionsByTagId.cend()) {
                    throw std::invalid_argument(QStringLiteral("Unknown TagId: %1").arg(configuration.tagId).toStdString());
                }
                Monitor::Application::Configuration::ConfigurationValidation::validateTag(
                    definitionIt.value(),
                    configuration);
            }
        }

        m_saveTagConfigurations(configurations);
        m_tagRuntimeConfigurationStore->replace(configurations);
        const auto synchronizedConfigurations = currentTagConfigurations();
        m_alarmService->replaceConfigurations(synchronizedConfigurations);
        if (!m_tagDefinitions.isEmpty()) {
            m_historyRuntimeStateConsumer->replaceConfigurations(m_tagDefinitions, synchronizedConfigurations);
        }
        m_operationLogService->write(
            Monitor::Domain::Logs::OperationLogLevel::Info,
            QStringLiteral("Settings"),
            QStringLiteral("TagConfigurations.Save"),
            QStringLiteral("RuntimeCommandFacade"),
            QStringLiteral("Tag runtime configurations saved and synchronized."),
            QStringLiteral("Count=%1").arg(synchronizedConfigurations.size()));
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

QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> RuntimeCommandFacade::currentTagConfigurations() const
{
    const auto snapshot = m_tagRuntimeConfigurationStore->snapshot();
    QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> configurations;
    configurations.reserve(snapshot.size());
    for (auto it = snapshot.cbegin(); it != snapshot.cend(); ++it) {
        configurations.append(it.value());
    }
    return configurations;
}

} // namespace Monitor::Application::Services
