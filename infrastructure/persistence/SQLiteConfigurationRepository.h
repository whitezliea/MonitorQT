#ifndef SQLITECONFIGURATIONREPOSITORY_H
#define SQLITECONFIGURATIONREPOSITORY_H

#include "application/configuration/TagRuntimeConfiguration.h"
#include "infrastructure/persistence/SqliteConnectionFactory.h"

#include <QHash>
#include <QString>
#include <QVector>

namespace Monitor::Infrastructure::Persistence {

class SQLiteConfigurationRepository
{
public:
    explicit SQLiteConfigurationRepository(SqliteConnectionFactory *connectionFactory);

    QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> loadTagConfigurations();
    void saveTagConfigurations(
        const QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> &configurations);

    QHash<QString, QString> loadRuntimeSettings();
    void saveRuntimeSettings(const QHash<QString, QString> &settings);

private:
    SqliteConnectionFactory *m_connectionFactory = nullptr;
};

} // namespace Monitor::Infrastructure::Persistence

#endif // SQLITECONFIGURATIONREPOSITORY_H
