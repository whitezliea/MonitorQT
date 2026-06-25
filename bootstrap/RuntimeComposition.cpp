#include "RuntimeComposition.h"

#include "EventRegistration.h"

#include "application/ApplicationLayer.h"
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

#include "phase0/SourceBehaviorFreeze.h"

#include <QFileInfo>

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
                QString(),
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
            m_historyRetentionService = std::make_unique<Monitor::Infrastructure::Persistence::HistoryRetentionService>(
                m_historyRepository.get(),
                m_operationLogRepository.get(),
                m_dependencies.runtimeOptions.historyRetentionDays,
                m_dependencies.runtimeOptions.historyRetentionDeleteBatchSize);
        } catch (const std::exception &exception) {
            errors->append(QStringLiteral("SQLite persistence initialization failed: %1").arg(QString::fromUtf8(exception.what())));
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
