#ifndef RUNTIMECOMPOSITION_H
#define RUNTIMECOMPOSITION_H

#include "RuntimeCompositionDependencies.h"

#include "application/events/EventBus.h"

#include <QStringList>

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
    QStringList layerTargetNames() const;
    QStringList registeredConsumerNames() const;

private:
    RuntimeCompositionDependencies m_dependencies;
    std::unique_ptr<Monitor::Application::EventBus> m_eventBus;
    std::unique_ptr<Monitor::Infrastructure::Persistence::SqliteConnectionFactory> m_sqliteConnectionFactory;
    std::unique_ptr<Monitor::Infrastructure::Persistence::SQLiteHistoryRepository> m_historyRepository;
    std::unique_ptr<Monitor::Infrastructure::Persistence::SQLiteAlarmRepository> m_alarmRepository;
    std::unique_ptr<Monitor::Infrastructure::Persistence::SQLiteOperationLogRepository> m_operationLogRepository;
    std::unique_ptr<Monitor::Infrastructure::Persistence::SQLiteConfigurationRepository> m_configurationRepository;
    std::unique_ptr<Monitor::Infrastructure::Persistence::HistoryRetentionService> m_historyRetentionService;
    QStringList m_layerTargetNames;
    bool m_initialized = false;
};

} // namespace Monitor::Bootstrap

#endif // RUNTIMECOMPOSITION_H
