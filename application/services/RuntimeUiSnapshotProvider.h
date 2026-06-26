#ifndef RUNTIMEUISNAPSHOTPROVIDER_H
#define RUNTIMEUISNAPSHOTPROVIDER_H

#include "application/configuration/RuntimeOptionsStore.h"
#include "application/configuration/TagRuntimeConfiguration.h"
#include "application/dto/ApplicationDtos.h"
#include "application/runtime/DataSourceHealthMonitor.h"
#include "application/runtime/PersistenceRuntimeCoordinator.h"
#include "application/runtime/RuntimeLifecycleCoordinator.h"
#include "application/services/AlarmService.h"
#include "application/services/DashboardService.h"
#include "application/services/MeasurementMapService.h"
#include "application/services/TagService.h"

namespace Monitor::Application::Services {

class RuntimeUiSnapshotProvider
{
public:
    RuntimeUiSnapshotProvider(
        const Monitor::Application::Runtime::RuntimeLifecycleCoordinator *runtimeLifecycle,
        const Monitor::Application::Runtime::PersistenceRuntimeCoordinator *persistenceRuntime,
        const Monitor::Application::Runtime::DataSourceHealthMonitor *dataSourceHealthMonitor,
        const Monitor::Application::Configuration::RuntimeOptionsStore *runtimeOptionsStore,
        const Monitor::Application::Configuration::TagRuntimeConfigurationStore *tagRuntimeConfigurationStore,
        const TagService *tagService,
        const AlarmService *alarmService,
        const DashboardService *dashboardService,
        const MeasurementMapService *measurementMapService,
        QVector<Monitor::Domain::Tags::TagDefinition> tagDefinitions,
        bool databaseAvailable);

    Monitor::Application::Dtos::UiSnapshot refresh() const;

private:
    QString syncState(
        const Monitor::Application::Runtime::RuntimeLifecycleStatus &runtimeStatus,
        const Monitor::Application::Runtime::PersistenceRuntimeStatus &persistenceStatus,
        const Monitor::Application::Runtime::DataSourceHealthStatus &healthStatus) const;
    QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> tagConfigurations() const;
    quint64 latestSequenceNo(const Monitor::Domain::Tags::TagSnapshot &tagSnapshot) const;

    const Monitor::Application::Runtime::RuntimeLifecycleCoordinator *m_runtimeLifecycle = nullptr;
    const Monitor::Application::Runtime::PersistenceRuntimeCoordinator *m_persistenceRuntime = nullptr;
    const Monitor::Application::Runtime::DataSourceHealthMonitor *m_dataSourceHealthMonitor = nullptr;
    const Monitor::Application::Configuration::RuntimeOptionsStore *m_runtimeOptionsStore = nullptr;
    const Monitor::Application::Configuration::TagRuntimeConfigurationStore *m_tagRuntimeConfigurationStore = nullptr;
    const TagService *m_tagService = nullptr;
    const AlarmService *m_alarmService = nullptr;
    const DashboardService *m_dashboardService = nullptr;
    const MeasurementMapService *m_measurementMapService = nullptr;
    QVector<Monitor::Domain::Tags::TagDefinition> m_tagDefinitions;
    bool m_databaseAvailable = false;
};

} // namespace Monitor::Application::Services

#endif // RUNTIMEUISNAPSHOTPROVIDER_H
