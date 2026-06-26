#include "RuntimeComposition.h"

#include "EventRegistration.h"

#include "application/ApplicationLayer.h"
#include "application/configuration/ConfigurationValidation.h"
#include "domain/DomainLayer.h"
#include "infrastructure/InfrastructureLayer.h"
#include "infrastructure/persistence/HistoryRetentionService.h"
#include "infrastructure/persistence/SQLiteAlarmRepository.h"
#include "infrastructure/persistence/SQLiteConfigurationRepository.h"
#include "infrastructure/persistence/SQLiteHistoryRepository.h"
#include "infrastructure/persistence/SQLiteOperationLogRepository.h"
#include "infrastructure/persistence/SqliteConnectionFactory.h"
#include "presentation/PresentationLayer.h"
#include "simulator/SimulatorLayer.h"
#include "application/services/TagDefinitionCatalog.h"

#include "phase0/SourceBehaviorFreeze.h"

#include <QFileInfo>
#include <QHash>

#include <stdexcept>
#include <utility>

namespace Monitor::Bootstrap {
namespace {

void appendErrors(QStringList *target, const QStringList &source)
{
    if (target) {
        target->append(source);
    }
}

QStringList collectLayerTargetNames()
{
    return {
        Monitor::Domain::domainLayerInfo().name,
        Monitor::Application::applicationLayerInfo().name,
        Monitor::Infrastructure::infrastructureLayerInfo().name,
        Monitor::Simulator::simulatorLayerInfo().name,
        Monitor::Presentation::presentationLayerInfo().name
    };
}

QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> defaultTagConfigurations(
    const QVector<Monitor::Domain::Tags::TagDefinition> &definitions)
{
    QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> configurations;
    configurations.reserve(definitions.size());
    for (const auto &definition : definitions) {
        configurations.append(Monitor::Application::Configuration::TagRuntimeConfiguration::fromDefinition(definition));
    }
    return configurations;
}

int settingInt(
    const QHash<QString, QString> &settings,
    const QString &key,
    int fallback)
{
    const auto it = settings.constFind(key);
    if (it == settings.cend()) {
        return fallback;
    }

    bool ok = false;
    const auto value = it.value().toInt(&ok);
    if (!ok) {
        throw std::invalid_argument(QStringLiteral("Runtime setting %1 must be an integer.").arg(key).toStdString());
    }

    return value;
}

Monitor::Application::Configuration::MonitorRuntimeOptions applyRuntimeSettings(
    Monitor::Application::Configuration::MonitorRuntimeOptions options,
    const QHash<QString, QString> &settings)
{
    using Monitor::Application::Configuration::RuntimeSettingKeys;

    options.dataGenerateIntervalMs = settingInt(settings, RuntimeSettingKeys::DataGenerateIntervalMs, options.dataGenerateIntervalMs);
    options.dataSourceTimeoutPeriods = settingInt(settings, RuntimeSettingKeys::DataSourceTimeoutPeriods, options.dataSourceTimeoutPeriods);
    options.uiRefreshIntervalMs = settingInt(settings, RuntimeSettingKeys::UiRefreshIntervalMs, options.uiRefreshIntervalMs);
    options.historyBatchIntervalMs = settingInt(settings, RuntimeSettingKeys::HistoryBatchIntervalMs, options.historyBatchIntervalMs);
    options.historyRetentionDays = settingInt(settings, RuntimeSettingKeys::HistoryRetentionDays, options.historyRetentionDays);
    options.alarmBatchIntervalMs = settingInt(settings, RuntimeSettingKeys::AlarmBatchIntervalMs, options.alarmBatchIntervalMs);
    options.operationLogBatchIntervalMs = settingInt(settings, RuntimeSettingKeys::OperationLogBatchIntervalMs, options.operationLogBatchIntervalMs);
    const auto maximumTrendWindow = settingInt(settings, RuntimeSettingKeys::MaximumTrendWindowMinutes, options.maximumTrendWindowMinutes());
    if (settings.contains(RuntimeSettingKeys::MaximumTrendWindowMinutes)) {
        options.trendWindowMinutes = {maximumTrendWindow};
    }

    Monitor::Application::Configuration::ConfigurationValidation::validateRuntimeOptions(options);
    return options;
}

QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> mergeTagConfigurations(
    const QVector<Monitor::Domain::Tags::TagDefinition> &definitions,
    const QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> &loaded)
{
    auto configurations = defaultTagConfigurations(definitions);
    if (loaded.isEmpty()) {
        return configurations;
    }

    QHash<QString, Monitor::Application::Configuration::TagRuntimeConfiguration> loadedByTagId;
    for (const auto &configuration : loaded) {
        loadedByTagId.insert(configuration.tagId, configuration);
    }

    for (auto &configuration : configurations) {
        const auto it = loadedByTagId.constFind(configuration.tagId);
        if (it != loadedByTagId.cend()) {
            configuration = it.value();
        }
    }

    return configurations;
}

} // namespace

RuntimeComposition::RuntimeComposition()
    : RuntimeComposition(RuntimeCompositionDependencies::createDefault())
{
}

RuntimeComposition::RuntimeComposition(RuntimeCompositionDependencies dependencies)
    : m_dependencies(std::move(dependencies))
{
}

RuntimeComposition::~RuntimeComposition() = default;
RuntimeComposition::RuntimeComposition(RuntimeComposition &&) noexcept = default;
RuntimeComposition &RuntimeComposition::operator=(RuntimeComposition &&) noexcept = default;

bool RuntimeComposition::initialize(QStringList *errors)
{
    QStringList localErrors;
    if (!errors) {
        errors = &localErrors;
    }

    errors->clear();

    QStringList phase0Errors;
    if (!Phase0::validateSourceBehaviorFreeze(&phase0Errors)) {
        appendErrors(errors, phase0Errors);
    }

    appendErrors(errors, Monitor::Domain::validateDomainLayer());
    appendErrors(errors, Monitor::Application::validateApplicationLayer());
    appendErrors(errors, Monitor::Infrastructure::validateInfrastructureLayer());
    appendErrors(errors, Monitor::Simulator::validateSimulatorLayer());

    appendErrors(errors, m_dependencies.validate());

    if (m_dependencies.useSqlitePersistence) {
        try {
            m_sqliteConnectionFactory = std::make_unique<Monitor::Infrastructure::Persistence::SqliteConnectionFactory>(
                m_dependencies.databasePath,
                m_dependencies.databaseDriverName);
            m_sqliteConnectionFactory->initialize();
            if (!QFileInfo::exists(m_sqliteConnectionFactory->databasePath())) {
                throw std::runtime_error(
                    QStringLiteral("SQLite database file was not created at %1.")
                        .arg(m_sqliteConnectionFactory->databasePath())
                        .toStdString());
            }
            if (m_sqliteConnectionFactory->schemaVersion() !=
                Monitor::Infrastructure::Persistence::SqliteConnectionFactory::currentSchemaVersion()) {
                throw std::runtime_error("SQLite database schema version is not current.");
            }
            m_historyRepository = std::make_unique<Monitor::Infrastructure::Persistence::SQLiteHistoryRepository>(
                m_sqliteConnectionFactory.get());
            m_alarmRepository = std::make_unique<Monitor::Infrastructure::Persistence::SQLiteAlarmRepository>(
                m_sqliteConnectionFactory.get());
            m_operationLogRepository = std::make_unique<Monitor::Infrastructure::Persistence::SQLiteOperationLogRepository>(
                m_sqliteConnectionFactory.get());
            m_configurationRepository = std::make_unique<Monitor::Infrastructure::Persistence::SQLiteConfigurationRepository>(
                m_sqliteConnectionFactory.get());
            m_runtimeOptions = applyRuntimeSettings(
                m_dependencies.runtimeOptions,
                m_configurationRepository->loadRuntimeSettings());
            m_tagDefinitions = Monitor::Application::Services::TagDefinitionCatalog::createDefaults();
            m_tagSourceMappings = Monitor::Application::Services::TagDefinitionCatalog::createSourceMappings(m_dependencies.defaultDeviceId);
            m_tagRuntimeConfigurations = mergeTagConfigurations(
                m_tagDefinitions,
                m_configurationRepository->loadTagConfigurations());
            m_historyRetentionService = std::make_unique<Monitor::Infrastructure::Persistence::HistoryRetentionService>(
                m_historyRepository.get(),
                m_operationLogRepository.get(),
                m_runtimeOptions.historyRetentionDays,
                m_runtimeOptions.historyRetentionDeleteBatchSize);
        } catch (const std::exception &exception) {
            errors->append(QStringLiteral("SQLite persistence initialization failed: %1").arg(QString::fromUtf8(exception.what())));
        }
    }

    if (!m_dependencies.useSqlitePersistence) {
        try {
            m_runtimeOptions = m_dependencies.runtimeOptions;
            Monitor::Application::Configuration::ConfigurationValidation::validateRuntimeOptions(m_runtimeOptions);
            m_tagDefinitions = Monitor::Application::Services::TagDefinitionCatalog::createDefaults();
            m_tagSourceMappings = Monitor::Application::Services::TagDefinitionCatalog::createSourceMappings(m_dependencies.defaultDeviceId);
            m_tagRuntimeConfigurations = defaultTagConfigurations(m_tagDefinitions);
        } catch (const std::exception &exception) {
            errors->append(QStringLiteral("Runtime configuration initialization failed: %1").arg(QString::fromUtf8(exception.what())));
        }
    }

    m_layerTargetNames = collectLayerTargetNames();
    if (m_layerTargetNames.size() != 5 ||
        !m_layerTargetNames.contains(QStringLiteral("MonitorDomain")) ||
        !m_layerTargetNames.contains(QStringLiteral("MonitorApplication")) ||
        !m_layerTargetNames.contains(QStringLiteral("MonitorInfrastructure")) ||
        !m_layerTargetNames.contains(QStringLiteral("MonitorSimulator")) ||
        !m_layerTargetNames.contains(QStringLiteral("MonitorPresentation"))) {
        errors->append(QStringLiteral("Runtime composition must expose the five stage-1 CMake layer targets."));
    }

    m_eventBus = std::make_unique<Monitor::Application::EventBus>();
    EventRegistration::registerApplicationEvents(*m_eventBus, errors);

    if (errors->isEmpty()) {
        try {
            m_runtimeOptionsStore = std::make_unique<Monitor::Application::Configuration::RuntimeOptionsStore>(m_runtimeOptions);
            m_tagRuntimeConfigurationStore = std::make_unique<Monitor::Application::Configuration::TagRuntimeConfigurationStore>(
                m_tagRuntimeConfigurations);
            m_dataCleanPipeline = std::make_unique<Monitor::Application::Pipelines::DataCleanPipeline>(
                m_tagDefinitions,
                m_tagSourceMappings);
            m_alarmService = std::make_unique<Monitor::Application::Services::AlarmService>(
                m_tagDefinitions,
                m_tagRuntimeConfigurations);
            m_tagService = std::make_unique<Monitor::Application::Services::TagService>(
                m_runtimeOptions.trendBufferCapacity());
            m_dashboardService = std::make_unique<Monitor::Application::Services::DashboardService>(
                m_tagService.get(),
                m_alarmService.get());
            m_chartDataService = std::make_unique<Monitor::Application::Services::ChartDataService>(
                m_tagService.get());
            m_measurementMapService = std::make_unique<Monitor::Application::Services::MeasurementMapService>();
            m_historySampleQueue = std::make_unique<Monitor::Application::Queues::HistorySampleQueue>();
            m_alarmEventQueue = std::make_unique<Monitor::Application::Queues::AlarmEventQueue>();
            m_operationLogQueue = std::make_unique<Monitor::Application::Queues::OperationLogQueue>();
            m_operationLogService = std::make_unique<Monitor::Application::Services::OperationLogService>(
                m_operationLogQueue.get());
            m_historyQueryService = std::make_unique<Monitor::Application::Services::HistoryQueryService>(
                [this](const Monitor::Application::Services::HistoryQueryRequest &request) {
                    const auto result = m_historyRepository->query(Monitor::Infrastructure::Persistence::HistoryQuery{
                        request.tagId,
                        request.startTimeUtc,
                        request.endTimeUtc,
                        request.page,
                        request.pageSize,
                        request.descending
                            ? Monitor::Infrastructure::Persistence::HistorySortDirection::Descending
                            : Monitor::Infrastructure::Persistence::HistorySortDirection::Ascending
                    });
                    return Monitor::Application::Services::HistoryQueryPage{
                        result.items,
                        result.totalCount,
                        result.page,
                        result.pageSize
                    };
                });
            m_alarmQueryService = std::make_unique<Monitor::Application::Services::AlarmQueryService>(
                [this](const Monitor::Application::Services::AlarmHistoryQueryRequest &request) {
                    const auto result = m_alarmRepository->query(Monitor::Infrastructure::Persistence::AlarmQuery{
                        request.startTimeUtc,
                        request.endTimeUtc,
                        request.tagId,
                        request.level,
                        request.state,
                        request.page,
                        request.pageSize,
                        request.ascending
                            ? Monitor::Infrastructure::Persistence::AlarmSortDirection::Ascending
                            : Monitor::Infrastructure::Persistence::AlarmSortDirection::Descending
                    });
                    return Monitor::Application::Services::AlarmHistoryQueryPage{
                        result.items,
                        result.totalCount,
                        result.page,
                        result.pageSize
                    };
                });
            m_operationLogQueryService = std::make_unique<Monitor::Application::Services::OperationLogQueryService>(
                [this](const Monitor::Application::Services::OperationLogQueryRequest &request) {
                    const auto result = m_operationLogRepository->queryPage(Monitor::Domain::Logs::OperationLogQuery{
                        request.startTimeUtc,
                        request.endTimeUtc,
                        request.level,
                        request.categoryText,
                        request.pageSize,
                        request.page,
                        request.pageSize
                    });
                    return Monitor::Application::Services::OperationLogQueryPage{
                        result.items,
                        result.totalCount,
                        result.page,
                        result.pageSize
                    };
                });
            m_tagCacheConsumer = std::make_unique<Monitor::Application::Services::TagCacheConsumer>(
                m_tagService.get());
            m_measurementMapFrameConsumer = std::make_unique<Monitor::Application::Services::MeasurementMapFrameConsumer>(
                m_measurementMapService.get());
            m_historyRuntimeStateConsumer = std::make_unique<Monitor::Application::Services::HistoryRuntimeStateConsumer>(
                m_historySampleQueue.get(),
                m_tagDefinitions,
                m_tagRuntimeConfigurations);
            m_alarmEventConsumer = std::make_unique<Monitor::Application::Services::AlarmEventConsumer>(
                m_alarmEventQueue.get());
            m_alarmOperationLogConsumer = std::make_unique<Monitor::Application::Services::AlarmOperationLogConsumer>(
                m_operationLogService.get());
            m_dataSourceHealthOperationLogConsumer = std::make_unique<Monitor::Application::Services::DataSourceHealthOperationLogConsumer>(
                m_operationLogService.get());
            if (!Monitor::Application::Services::registerDefaultRuntimeConsumers(
                    m_eventBus.get(),
                    m_tagCacheConsumer.get(),
                    m_measurementMapFrameConsumer.get(),
                    m_historyRuntimeStateConsumer.get(),
                    m_alarmEventConsumer.get(),
                    m_alarmOperationLogConsumer.get(),
                    m_dataSourceHealthOperationLogConsumer.get(),
                    errors)) {
                throw std::runtime_error("Failed to register default runtime event consumers.");
            }

            m_dataSourceHealthMonitor = std::make_unique<Monitor::Application::Runtime::DataSourceHealthMonitor>();
            if (m_dependencies.useSimulatorDataSource) {
                m_simulatorDataSource = std::make_unique<Monitor::Simulator::Adapters::SimulatorDataSource>(
                    Monitor::Simulator::Generators::FakeDataGenerator(),
                    m_runtimeOptions);
            }

            if (!m_simulatorDataSource) {
                throw std::runtime_error("No raw frame data source is configured.");
            }

            m_monitoringRuntimeService = std::make_unique<Monitor::Application::Runtime::MonitoringRuntimeService>(
                m_simulatorDataSource.get(),
                m_dataCleanPipeline.get(),
                m_alarmService.get(),
                m_eventBus.get(),
                m_runtimeOptions,
                m_dataSourceHealthMonitor.get());

            if (!m_historyRepository || !m_alarmRepository || !m_operationLogRepository) {
                throw std::runtime_error("Persistence workers require SQLite repositories.");
            }

            m_historyPersistWorker =
                std::make_unique<Monitor::Application::Workers::BatchPersistWorker<Monitor::Domain::Tags::TagValue>>(
                    QStringLiteral("History"),
                    m_historySampleQueue.get(),
                    m_runtimeOptions.historyBatchIntervalMs,
                    m_runtimeOptions.historyMaxBatchSize,
                    [this](const QVector<Monitor::Domain::Tags::TagValue> &samples) {
                        m_historyRepository->append(samples);
                    });
            m_alarmPersistWorker =
                std::make_unique<Monitor::Application::Workers::BatchPersistWorker<Monitor::Domain::Alarms::AlarmEvent>>(
                    QStringLiteral("Alarm"),
                    m_alarmEventQueue.get(),
                    m_runtimeOptions.alarmBatchIntervalMs,
                    m_runtimeOptions.alarmMaxBatchSize,
                    [this](const QVector<Monitor::Domain::Alarms::AlarmEvent> &alarms) {
                        m_alarmRepository->append(alarms);
                    });
            m_operationLogPersistWorker =
                std::make_unique<Monitor::Application::Workers::BatchPersistWorker<Monitor::Domain::Logs::OperationLog>>(
                    QStringLiteral("OperationLog"),
                    m_operationLogQueue.get(),
                    m_runtimeOptions.operationLogBatchIntervalMs,
                    m_runtimeOptions.operationLogMaxBatchSize,
                    [this](const QVector<Monitor::Domain::Logs::OperationLog> &logs) {
                        m_operationLogRepository->append(logs);
                    });
            m_persistenceRuntimeCoordinator = std::make_unique<Monitor::Application::Runtime::PersistenceRuntimeCoordinator>(
                QVector<Monitor::Application::Workers::IPersistWorker *>{
                    m_historyPersistWorker.get(),
                    m_alarmPersistWorker.get(),
                    m_operationLogPersistWorker.get()
                });
            m_runtimeLifecycleCoordinator = std::make_unique<Monitor::Application::Runtime::RuntimeLifecycleCoordinator>(
                [this](std::atomic_bool &stopRequested) {
                    m_monitoringRuntimeService->run(stopRequested);
                });
            m_acquisitionRuntimeController = std::make_unique<Monitor::Application::Runtime::AcquisitionRuntimeController>(
                m_runtimeLifecycleCoordinator.get(),
                m_persistenceRuntimeCoordinator.get(),
                m_operationLogService.get());
            m_runtimeCommandFacade = std::make_unique<Monitor::Application::Services::RuntimeCommandFacade>(
                m_acquisitionRuntimeController.get(),
                m_monitoringRuntimeService.get(),
                m_runtimeOptionsStore.get(),
                m_tagRuntimeConfigurationStore.get(),
                m_alarmService.get(),
                m_historyRuntimeStateConsumer.get(),
                m_operationLogService.get(),
                m_tagDefinitions,
                [this](const QHash<QString, QString> &settings) {
                    m_configurationRepository->saveRuntimeSettings(settings);
                },
                [this](const QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> &configurations) {
                    m_configurationRepository->saveTagConfigurations(configurations);
                });
            m_runtimeUiSnapshotProvider = std::make_unique<Monitor::Application::Services::RuntimeUiSnapshotProvider>(
                m_runtimeLifecycleCoordinator.get(),
                m_persistenceRuntimeCoordinator.get(),
                m_dataSourceHealthMonitor.get(),
                m_runtimeOptionsStore.get(),
                m_tagRuntimeConfigurationStore.get(),
                m_tagService.get(),
                m_alarmService.get(),
                m_dashboardService.get(),
                m_measurementMapService.get(),
                m_tagDefinitions,
                m_sqliteConnectionFactory != nullptr);
            m_applicationRuntimeHost = std::make_unique<ApplicationRuntimeHost>(
                m_runtimeLifecycleCoordinator.get(),
                m_persistenceRuntimeCoordinator.get(),
                m_historyRetentionService.get(),
                m_operationLogService.get());
        } catch (const std::exception &exception) {
            errors->append(QStringLiteral("Runtime object graph initialization failed: %1").arg(QString::fromUtf8(exception.what())));
        }
    }

    m_initialized = errors->isEmpty();
    return m_initialized;
}

bool RuntimeComposition::isInitialized() const
{
    return m_initialized;
}

const RuntimeCompositionDependencies &RuntimeComposition::dependencies() const
{
    return m_dependencies;
}

Monitor::Application::EventBus *RuntimeComposition::eventBus()
{
    return m_eventBus.get();
}

const Monitor::Application::EventBus *RuntimeComposition::eventBus() const
{
    return m_eventBus.get();
}

Monitor::Infrastructure::Persistence::SqliteConnectionFactory *RuntimeComposition::sqliteConnectionFactory()
{
    return m_sqliteConnectionFactory.get();
}

const Monitor::Infrastructure::Persistence::SqliteConnectionFactory *RuntimeComposition::sqliteConnectionFactory() const
{
    return m_sqliteConnectionFactory.get();
}

Monitor::Infrastructure::Persistence::SQLiteHistoryRepository *RuntimeComposition::historyRepository()
{
    return m_historyRepository.get();
}

Monitor::Infrastructure::Persistence::SQLiteAlarmRepository *RuntimeComposition::alarmRepository()
{
    return m_alarmRepository.get();
}

Monitor::Infrastructure::Persistence::SQLiteOperationLogRepository *RuntimeComposition::operationLogRepository()
{
    return m_operationLogRepository.get();
}

Monitor::Infrastructure::Persistence::SQLiteConfigurationRepository *RuntimeComposition::configurationRepository()
{
    return m_configurationRepository.get();
}

Monitor::Infrastructure::Persistence::HistoryRetentionService *RuntimeComposition::historyRetentionService()
{
    return m_historyRetentionService.get();
}

const QVector<Monitor::Domain::Tags::TagDefinition> &RuntimeComposition::tagDefinitions() const
{
    return m_tagDefinitions;
}

const QVector<Monitor::Domain::Tags::TagSourceMapping> &RuntimeComposition::tagSourceMappings() const
{
    return m_tagSourceMappings;
}

const QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> &RuntimeComposition::tagRuntimeConfigurations() const
{
    return m_tagRuntimeConfigurations;
}

const Monitor::Application::Configuration::MonitorRuntimeOptions &RuntimeComposition::runtimeOptions() const
{
    return m_runtimeOptions;
}

Monitor::Application::Configuration::RuntimeOptionsStore *RuntimeComposition::runtimeOptionsStore()
{
    return m_runtimeOptionsStore.get();
}

Monitor::Application::Configuration::TagRuntimeConfigurationStore *RuntimeComposition::tagRuntimeConfigurationStore()
{
    return m_tagRuntimeConfigurationStore.get();
}

Monitor::Application::Pipelines::DataCleanPipeline *RuntimeComposition::dataCleanPipeline()
{
    return m_dataCleanPipeline.get();
}

Monitor::Application::Services::AlarmService *RuntimeComposition::alarmService()
{
    return m_alarmService.get();
}

Monitor::Application::Services::TagService *RuntimeComposition::tagService()
{
    return m_tagService.get();
}

Monitor::Application::Services::DashboardService *RuntimeComposition::dashboardService()
{
    return m_dashboardService.get();
}

Monitor::Application::Services::ChartDataService *RuntimeComposition::chartDataService()
{
    return m_chartDataService.get();
}

Monitor::Application::Services::MeasurementMapService *RuntimeComposition::measurementMapService()
{
    return m_measurementMapService.get();
}

Monitor::Application::Services::OperationLogService *RuntimeComposition::operationLogService()
{
    return m_operationLogService.get();
}

Monitor::Application::Services::HistoryQueryService *RuntimeComposition::historyQueryService()
{
    return m_historyQueryService.get();
}

Monitor::Application::Services::AlarmQueryService *RuntimeComposition::alarmQueryService()
{
    return m_alarmQueryService.get();
}

Monitor::Application::Services::OperationLogQueryService *RuntimeComposition::operationLogQueryService()
{
    return m_operationLogQueryService.get();
}

Monitor::Application::Services::RuntimeCommandFacade *RuntimeComposition::runtimeCommandFacade()
{
    return m_runtimeCommandFacade.get();
}

Monitor::Application::Services::RuntimeUiSnapshotProvider *RuntimeComposition::runtimeUiSnapshotProvider()
{
    return m_runtimeUiSnapshotProvider.get();
}

Monitor::Application::Services::TagCacheConsumer *RuntimeComposition::tagCacheConsumer()
{
    return m_tagCacheConsumer.get();
}

Monitor::Application::Services::MeasurementMapFrameConsumer *RuntimeComposition::measurementMapFrameConsumer()
{
    return m_measurementMapFrameConsumer.get();
}

Monitor::Application::Services::HistoryRuntimeStateConsumer *RuntimeComposition::historyRuntimeStateConsumer()
{
    return m_historyRuntimeStateConsumer.get();
}

Monitor::Application::Services::AlarmEventConsumer *RuntimeComposition::alarmEventConsumer()
{
    return m_alarmEventConsumer.get();
}

Monitor::Application::Services::AlarmOperationLogConsumer *RuntimeComposition::alarmOperationLogConsumer()
{
    return m_alarmOperationLogConsumer.get();
}

Monitor::Application::Services::DataSourceHealthOperationLogConsumer *RuntimeComposition::dataSourceHealthOperationLogConsumer()
{
    return m_dataSourceHealthOperationLogConsumer.get();
}

Monitor::Application::Runtime::DataSourceHealthMonitor *RuntimeComposition::dataSourceHealthMonitor()
{
    return m_dataSourceHealthMonitor.get();
}

Monitor::Simulator::Adapters::SimulatorDataSource *RuntimeComposition::simulatorDataSource()
{
    return m_simulatorDataSource.get();
}

Monitor::Application::Runtime::MonitoringRuntimeService *RuntimeComposition::monitoringRuntimeService()
{
    return m_monitoringRuntimeService.get();
}

Monitor::Application::Queues::HistorySampleQueue *RuntimeComposition::historySampleQueue()
{
    return m_historySampleQueue.get();
}

Monitor::Application::Queues::AlarmEventQueue *RuntimeComposition::alarmEventQueue()
{
    return m_alarmEventQueue.get();
}

Monitor::Application::Queues::OperationLogQueue *RuntimeComposition::operationLogQueue()
{
    return m_operationLogQueue.get();
}

Monitor::Application::Workers::IPersistWorker *RuntimeComposition::historyPersistWorker()
{
    return m_historyPersistWorker.get();
}

Monitor::Application::Workers::IPersistWorker *RuntimeComposition::alarmPersistWorker()
{
    return m_alarmPersistWorker.get();
}

Monitor::Application::Workers::IPersistWorker *RuntimeComposition::operationLogPersistWorker()
{
    return m_operationLogPersistWorker.get();
}

Monitor::Application::Runtime::PersistenceRuntimeCoordinator *RuntimeComposition::persistenceRuntimeCoordinator()
{
    return m_persistenceRuntimeCoordinator.get();
}

Monitor::Application::Runtime::RuntimeLifecycleCoordinator *RuntimeComposition::runtimeLifecycleCoordinator()
{
    return m_runtimeLifecycleCoordinator.get();
}

Monitor::Application::Runtime::AcquisitionRuntimeController *RuntimeComposition::acquisitionRuntimeController()
{
    return m_acquisitionRuntimeController.get();
}

ApplicationRuntimeHost *RuntimeComposition::applicationRuntimeHost()
{
    return m_applicationRuntimeHost.get();
}

QStringList RuntimeComposition::layerTargetNames() const
{
    return m_layerTargetNames;
}

QStringList RuntimeComposition::registeredConsumerNames() const
{
    QStringList consumers;
    if (!m_eventBus) {
        return consumers;
    }

    for (const auto &subscription : m_eventBus->subscriptions()) {
        if (!consumers.contains(subscription.consumerName)) {
            consumers.append(subscription.consumerName);
        }
    }

    return consumers;
}

} // namespace Monitor::Bootstrap
