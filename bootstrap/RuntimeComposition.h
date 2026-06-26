#ifndef RUNTIMECOMPOSITION_H
#define RUNTIMECOMPOSITION_H

#include "RuntimeCompositionDependencies.h"

#include "application/configuration/RuntimeOptionsStore.h"
#include "application/configuration/TagRuntimeConfiguration.h"
#include "application/events/EventBus.h"
#include "application/pipelines/DataCleanPipeline.h"
#include "application/queues/ApplicationQueues.h"
#include "application/runtime/AcquisitionRuntimeController.h"
#include "application/runtime/DataSourceHealthMonitor.h"
#include "application/runtime/MonitoringRuntimeService.h"
#include "application/runtime/PersistenceRuntimeCoordinator.h"
#include "application/runtime/RuntimeLifecycleCoordinator.h"
#include "application/services/AlarmService.h"
#include "application/services/ChartDataService.h"
#include "application/services/DashboardService.h"
#include "application/services/MeasurementMapService.h"
#include "application/services/OperationLogService.h"
#include "application/services/RuntimeEventConsumers.h"
#include "application/services/TagService.h"
#include "application/workers/BatchPersistWorker.h"
#include "simulator/adapters/SimulatorDataSource.h"

#include <QStringList>
#include <QVector>

#include <memory>

namespace Monitor::Infrastructure::Persistence {

class HistoryRetentionService;
class SQLiteAlarmRepository;
class SQLiteConfigurationRepository;
class SQLiteHistoryRepository;
class SQLiteOperationLogRepository;
class SqliteConnectionFactory;

} // namespace Monitor::Infrastructure::Persistence

namespace Monitor::Bootstrap {

class RuntimeComposition
{
public:
    RuntimeComposition();
    explicit RuntimeComposition(RuntimeCompositionDependencies dependencies);
    ~RuntimeComposition();

    RuntimeComposition(const RuntimeComposition &) = delete;
    RuntimeComposition &operator=(const RuntimeComposition &) = delete;

    RuntimeComposition(RuntimeComposition &&) noexcept;
    RuntimeComposition &operator=(RuntimeComposition &&) noexcept;

    bool initialize(QStringList *errors = nullptr);
    bool isInitialized() const;

    const RuntimeCompositionDependencies &dependencies() const;
    Monitor::Application::EventBus *eventBus();
    const Monitor::Application::EventBus *eventBus() const;
    Monitor::Infrastructure::Persistence::SqliteConnectionFactory *sqliteConnectionFactory();
    const Monitor::Infrastructure::Persistence::SqliteConnectionFactory *sqliteConnectionFactory() const;
    Monitor::Infrastructure::Persistence::SQLiteHistoryRepository *historyRepository();
    Monitor::Infrastructure::Persistence::SQLiteAlarmRepository *alarmRepository();
    Monitor::Infrastructure::Persistence::SQLiteOperationLogRepository *operationLogRepository();
    Monitor::Infrastructure::Persistence::SQLiteConfigurationRepository *configurationRepository();
    Monitor::Infrastructure::Persistence::HistoryRetentionService *historyRetentionService();
    const QVector<Monitor::Domain::Tags::TagDefinition> &tagDefinitions() const;
    const QVector<Monitor::Domain::Tags::TagSourceMapping> &tagSourceMappings() const;
    const QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> &tagRuntimeConfigurations() const;
    const Monitor::Application::Configuration::MonitorRuntimeOptions &runtimeOptions() const;
    Monitor::Application::Configuration::RuntimeOptionsStore *runtimeOptionsStore();
    Monitor::Application::Configuration::TagRuntimeConfigurationStore *tagRuntimeConfigurationStore();
    Monitor::Application::Pipelines::DataCleanPipeline *dataCleanPipeline();
    Monitor::Application::Services::AlarmService *alarmService();
    Monitor::Application::Services::TagService *tagService();
    Monitor::Application::Services::DashboardService *dashboardService();
    Monitor::Application::Services::ChartDataService *chartDataService();
    Monitor::Application::Services::MeasurementMapService *measurementMapService();
    Monitor::Application::Services::OperationLogService *operationLogService();
    Monitor::Application::Services::TagCacheConsumer *tagCacheConsumer();
    Monitor::Application::Services::MeasurementMapFrameConsumer *measurementMapFrameConsumer();
    Monitor::Application::Services::HistoryRuntimeStateConsumer *historyRuntimeStateConsumer();
    Monitor::Application::Services::AlarmEventConsumer *alarmEventConsumer();
    Monitor::Application::Services::AlarmOperationLogConsumer *alarmOperationLogConsumer();
    Monitor::Application::Services::DataSourceHealthOperationLogConsumer *dataSourceHealthOperationLogConsumer();
    Monitor::Application::Runtime::DataSourceHealthMonitor *dataSourceHealthMonitor();
    Monitor::Simulator::Adapters::SimulatorDataSource *simulatorDataSource();
    Monitor::Application::Runtime::MonitoringRuntimeService *monitoringRuntimeService();
    Monitor::Application::Queues::HistorySampleQueue *historySampleQueue();
    Monitor::Application::Queues::AlarmEventQueue *alarmEventQueue();
    Monitor::Application::Queues::OperationLogQueue *operationLogQueue();
    Monitor::Application::Workers::IPersistWorker *historyPersistWorker();
    Monitor::Application::Workers::IPersistWorker *alarmPersistWorker();
    Monitor::Application::Workers::IPersistWorker *operationLogPersistWorker();
    Monitor::Application::Runtime::PersistenceRuntimeCoordinator *persistenceRuntimeCoordinator();
    Monitor::Application::Runtime::RuntimeLifecycleCoordinator *runtimeLifecycleCoordinator();
    Monitor::Application::Runtime::AcquisitionRuntimeController *acquisitionRuntimeController();
    QStringList layerTargetNames() const;
    QStringList registeredConsumerNames() const;

private:
    RuntimeCompositionDependencies m_dependencies;
    QVector<Monitor::Domain::Tags::TagDefinition> m_tagDefinitions;
    QVector<Monitor::Domain::Tags::TagSourceMapping> m_tagSourceMappings;
    QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> m_tagRuntimeConfigurations;
    Monitor::Application::Configuration::MonitorRuntimeOptions m_runtimeOptions;
    std::unique_ptr<Monitor::Application::EventBus> m_eventBus;
    std::unique_ptr<Monitor::Infrastructure::Persistence::SqliteConnectionFactory> m_sqliteConnectionFactory;
    std::unique_ptr<Monitor::Infrastructure::Persistence::SQLiteHistoryRepository> m_historyRepository;
    std::unique_ptr<Monitor::Infrastructure::Persistence::SQLiteAlarmRepository> m_alarmRepository;
    std::unique_ptr<Monitor::Infrastructure::Persistence::SQLiteOperationLogRepository> m_operationLogRepository;
    std::unique_ptr<Monitor::Infrastructure::Persistence::SQLiteConfigurationRepository> m_configurationRepository;
    std::unique_ptr<Monitor::Infrastructure::Persistence::HistoryRetentionService> m_historyRetentionService;
    std::unique_ptr<Monitor::Application::Configuration::RuntimeOptionsStore> m_runtimeOptionsStore;
    std::unique_ptr<Monitor::Application::Configuration::TagRuntimeConfigurationStore> m_tagRuntimeConfigurationStore;
    std::unique_ptr<Monitor::Application::Pipelines::DataCleanPipeline> m_dataCleanPipeline;
    std::unique_ptr<Monitor::Application::Services::AlarmService> m_alarmService;
    std::unique_ptr<Monitor::Application::Services::TagService> m_tagService;
    std::unique_ptr<Monitor::Application::Services::DashboardService> m_dashboardService;
    std::unique_ptr<Monitor::Application::Services::ChartDataService> m_chartDataService;
    std::unique_ptr<Monitor::Application::Services::MeasurementMapService> m_measurementMapService;
    std::unique_ptr<Monitor::Application::Queues::HistorySampleQueue> m_historySampleQueue;
    std::unique_ptr<Monitor::Application::Queues::AlarmEventQueue> m_alarmEventQueue;
    std::unique_ptr<Monitor::Application::Queues::OperationLogQueue> m_operationLogQueue;
    std::unique_ptr<Monitor::Application::Services::OperationLogService> m_operationLogService;
    std::unique_ptr<Monitor::Application::Services::TagCacheConsumer> m_tagCacheConsumer;
    std::unique_ptr<Monitor::Application::Services::MeasurementMapFrameConsumer> m_measurementMapFrameConsumer;
    std::unique_ptr<Monitor::Application::Services::HistoryRuntimeStateConsumer> m_historyRuntimeStateConsumer;
    std::unique_ptr<Monitor::Application::Services::AlarmEventConsumer> m_alarmEventConsumer;
    std::unique_ptr<Monitor::Application::Services::AlarmOperationLogConsumer> m_alarmOperationLogConsumer;
    std::unique_ptr<Monitor::Application::Services::DataSourceHealthOperationLogConsumer> m_dataSourceHealthOperationLogConsumer;
    std::unique_ptr<Monitor::Application::Runtime::DataSourceHealthMonitor> m_dataSourceHealthMonitor;
    std::unique_ptr<Monitor::Simulator::Adapters::SimulatorDataSource> m_simulatorDataSource;
    std::unique_ptr<Monitor::Application::Runtime::MonitoringRuntimeService> m_monitoringRuntimeService;
    std::unique_ptr<Monitor::Application::Workers::BatchPersistWorker<Monitor::Domain::Tags::TagValue>> m_historyPersistWorker;
    std::unique_ptr<Monitor::Application::Workers::BatchPersistWorker<Monitor::Domain::Alarms::AlarmEvent>> m_alarmPersistWorker;
    std::unique_ptr<Monitor::Application::Workers::BatchPersistWorker<Monitor::Domain::Logs::OperationLog>> m_operationLogPersistWorker;
    std::unique_ptr<Monitor::Application::Runtime::PersistenceRuntimeCoordinator> m_persistenceRuntimeCoordinator;
    std::unique_ptr<Monitor::Application::Runtime::RuntimeLifecycleCoordinator> m_runtimeLifecycleCoordinator;
    std::unique_ptr<Monitor::Application::Runtime::AcquisitionRuntimeController> m_acquisitionRuntimeController;
    QStringList m_layerTargetNames;
    bool m_initialized = false;
};

} // namespace Monitor::Bootstrap

#endif // RUNTIMECOMPOSITION_H
