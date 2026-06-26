#ifndef RUNTIMECOMMANDFACADE_H
#define RUNTIMECOMMANDFACADE_H

#include "application/configuration/MonitorRuntimeOptions.h"
#include "application/configuration/RuntimeOptionsStore.h"
#include "application/configuration/TagRuntimeConfiguration.h"
#include "application/runtime/AcquisitionRuntimeController.h"
#include "application/runtime/MonitoringRuntimeService.h"
#include "application/services/AlarmService.h"
#include "application/services/OperationLogService.h"
#include "domain/tags/TagModels.h"

#include <QHash>
#include <QStringList>
#include <QUuid>
#include <QVector>

#include <functional>

namespace Monitor::Application::Services {

class HistoryRuntimeStateConsumer;

class RuntimeCommandFacade
{
public:
    using SaveRuntimeSettingsFunction = std::function<void(const QHash<QString, QString> &)>;
    using SaveTagConfigurationsFunction =
        std::function<void(const QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> &)>;

    RuntimeCommandFacade(
        Monitor::Application::Runtime::AcquisitionRuntimeController *acquisitionController,
        Monitor::Application::Runtime::MonitoringRuntimeService *monitoringRuntimeService,
        Monitor::Application::Configuration::RuntimeOptionsStore *runtimeOptionsStore,
        Monitor::Application::Configuration::TagRuntimeConfigurationStore *tagRuntimeConfigurationStore,
        AlarmService *alarmService,
        HistoryRuntimeStateConsumer *historyRuntimeStateConsumer,
        OperationLogService *operationLogService);
    RuntimeCommandFacade(
        Monitor::Application::Runtime::AcquisitionRuntimeController *acquisitionController,
        Monitor::Application::Runtime::MonitoringRuntimeService *monitoringRuntimeService,
        Monitor::Application::Configuration::RuntimeOptionsStore *runtimeOptionsStore,
        Monitor::Application::Configuration::TagRuntimeConfigurationStore *tagRuntimeConfigurationStore,
        AlarmService *alarmService,
        HistoryRuntimeStateConsumer *historyRuntimeStateConsumer,
        OperationLogService *operationLogService,
        QVector<Monitor::Domain::Tags::TagDefinition> tagDefinitions,
        SaveRuntimeSettingsFunction saveRuntimeSettings,
        SaveTagConfigurationsFunction saveTagConfigurations);

    bool start(QStringList *errors = nullptr);
    bool stop(QStringList *errors = nullptr);
    bool acknowledgeAlarm(const QUuid &alarmId, QStringList *errors = nullptr);
    bool saveRuntimeOptions(
        const Monitor::Application::Configuration::MonitorRuntimeOptions &options,
        QStringList *errors = nullptr);
    bool saveTagConfigurations(
        const QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> &configurations,
        QStringList *errors = nullptr);

private:
    void appendError(QStringList *errors, const QString &message) const;
    QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> currentTagConfigurations() const;

    Monitor::Application::Runtime::AcquisitionRuntimeController *m_acquisitionController = nullptr;
    Monitor::Application::Runtime::MonitoringRuntimeService *m_monitoringRuntimeService = nullptr;
    Monitor::Application::Configuration::RuntimeOptionsStore *m_runtimeOptionsStore = nullptr;
    Monitor::Application::Configuration::TagRuntimeConfigurationStore *m_tagRuntimeConfigurationStore = nullptr;
    AlarmService *m_alarmService = nullptr;
    HistoryRuntimeStateConsumer *m_historyRuntimeStateConsumer = nullptr;
    OperationLogService *m_operationLogService = nullptr;
    QVector<Monitor::Domain::Tags::TagDefinition> m_tagDefinitions;
    SaveRuntimeSettingsFunction m_saveRuntimeSettings;
    SaveTagConfigurationsFunction m_saveTagConfigurations;
};

} // namespace Monitor::Application::Services

#endif // RUNTIMECOMMANDFACADE_H
