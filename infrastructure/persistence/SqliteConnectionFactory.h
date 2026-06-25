#ifndef SQLITECONNECTIONFACTORY_H
#define SQLITECONNECTIONFACTORY_H

#include <QMutex>
#include <QSqlDatabase>
#include <QString>

namespace Monitor::Infrastructure::Persistence {

class SqliteDatabaseConnection
{
public:
    SqliteDatabaseConnection() = default;
    SqliteDatabaseConnection(QString connectionName, QSqlDatabase database);
    ~SqliteDatabaseConnection();

    SqliteDatabaseConnection(const SqliteDatabaseConnection &) = delete;
    SqliteDatabaseConnection &operator=(const SqliteDatabaseConnection &) = delete;
    SqliteDatabaseConnection(SqliteDatabaseConnection &&other) noexcept;
    SqliteDatabaseConnection &operator=(SqliteDatabaseConnection &&other) noexcept;

    QSqlDatabase &database();
    const QSqlDatabase &database() const;
    bool isValid() const;

private:
    void close();

    QString m_connectionName;
    QSqlDatabase m_database;
};

class SqliteConnectionFactory
{
public:
    explicit SqliteConnectionFactory(
        const QString &databasePath = QString(),
        const QString &driverName = QStringLiteral("QSQLITE"));

    static int currentSchemaVersion();
    static QString defaultDatabasePath();

    QString databasePath() const;
    QString driverName() const;

    void initialize();
    SqliteDatabaseConnection openConnection();
    SqliteDatabaseConnection openReadConnection();
    int schemaVersion();
    QString journalMode();

private:
    SqliteDatabaseConnection openCore(bool readOnly);
    void executePragmas(QSqlDatabase &database, bool readOnly);

    QString m_databasePath;
    QString m_driverName;
    mutable QMutex m_mutex;
    bool m_initialized = false;
};

} // namespace Monitor::Infrastructure::Persistence

#endif // SQLITECONNECTIONFACTORY_H
