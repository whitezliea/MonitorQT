#include "SqliteConnectionFactory.h"

#include "SqliteSchemaMigrations.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>

#include <atomic>
#include <stdexcept>
#include <utility>

namespace Monitor::Infrastructure::Persistence {
namespace {

std::atomic<quint64> ConnectionCounter = 0;

void throwSql(const QString &message, const QSqlDatabase &database)
{
    throw std::runtime_error(QStringLiteral("%1: %2").arg(message, database.lastError().text()).toStdString());
}

void throwSql(const QString &message, const QSqlQuery &query)
{
    throw std::runtime_error(QStringLiteral("%1: %2").arg(message, query.lastError().text()).toStdString());
}

void execSql(QSqlDatabase &database, const QString &sql)
{
    QSqlQuery query(database);
    if (!query.exec(sql)) {
        throwSql(QStringLiteral("SQLite command failed"), query);
    }
}

QString createConnectionName()
{
    return QStringLiteral("monitorqt_sqlite_%1_%2")
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()))
        .arg(ConnectionCounter.fetch_add(1));
}

} // namespace

SqliteDatabaseConnection::SqliteDatabaseConnection(QString connectionName, QSqlDatabase database)
    : m_connectionName(std::move(connectionName)),
      m_database(std::move(database))
{
}

SqliteDatabaseConnection::~SqliteDatabaseConnection()
{
    close();
}

SqliteDatabaseConnection::SqliteDatabaseConnection(SqliteDatabaseConnection &&other) noexcept
    : m_connectionName(std::move(other.m_connectionName)),
      m_database(std::move(other.m_database))
{
    other.m_connectionName.clear();
    other.m_database = QSqlDatabase();
}

SqliteDatabaseConnection &SqliteDatabaseConnection::operator=(SqliteDatabaseConnection &&other) noexcept
{
    if (this != &other) {
        close();
        m_connectionName = std::move(other.m_connectionName);
        m_database = std::move(other.m_database);
        other.m_connectionName.clear();
        other.m_database = QSqlDatabase();
    }
    return *this;
}

QSqlDatabase &SqliteDatabaseConnection::database()
{
    return m_database;
}

const QSqlDatabase &SqliteDatabaseConnection::database() const
{
    return m_database;
}

bool SqliteDatabaseConnection::isValid() const
{
    return !m_connectionName.isEmpty() && m_database.isValid() && m_database.isOpen();
}

void SqliteDatabaseConnection::close()
{
    if (m_connectionName.isEmpty()) {
        return;
    }

    if (m_database.isValid()) {
        m_database.close();
    }
    const auto connectionName = m_connectionName;
    m_database = QSqlDatabase();
    m_connectionName.clear();
    QSqlDatabase::removeDatabase(connectionName);
}

SqliteConnectionFactory::SqliteConnectionFactory(
    const QString &databasePath,
    const QString &driverName)
    : m_databasePath(databasePath.trimmed().isEmpty() ? defaultDatabasePath() : QFileInfo(databasePath).absoluteFilePath()),
      m_driverName(driverName)
{
}

int SqliteConnectionFactory::currentSchemaVersion()
{
    return SqliteSchemaMigrations::currentVersion();
}

QString SqliteConnectionFactory::defaultDatabasePath()
{
    const auto baseDir = QCoreApplication::instance()
        ? QCoreApplication::applicationDirPath()
        : QDir::currentPath();
    return QDir(baseDir).filePath(QStringLiteral("data/multichannel-monitor.db"));
}

QString SqliteConnectionFactory::databasePath() const
{
    return m_databasePath;
}

QString SqliteConnectionFactory::driverName() const
{
    return m_driverName;
}

void SqliteConnectionFactory::initialize()
{
    QMutexLocker locker(&m_mutex);
    if (m_initialized) {
        return;
    }

    const auto directory = QFileInfo(m_databasePath).absoluteDir();
    if (!directory.exists() && !QDir().mkpath(directory.absolutePath())) {
        throw std::runtime_error(QStringLiteral("Failed to create SQLite database directory: %1").arg(directory.absolutePath()).toStdString());
    }

    auto connection = openCore(false);
    auto &database = connection.database();
    execSql(database, QStringLiteral("PRAGMA journal_mode=WAL;"));
    execSql(database, QStringLiteral("PRAGMA synchronous=NORMAL;"));
    execSql(database, QStringLiteral("PRAGMA busy_timeout=5000;"));
    execSql(database, QStringLiteral("PRAGMA foreign_keys=ON;"));

    SqliteSchemaMigrator migrator;
    const auto version = migrator.migrate(database);
    if (version != migrator.targetVersion()) {
        throw std::runtime_error("SQLite schema migration did not reach the target version.");
    }

    m_initialized = true;
}

SqliteDatabaseConnection SqliteConnectionFactory::openConnection()
{
    initialize();
    auto connection = openCore(false);
    executePragmas(connection.database(), false);
    return connection;
}

SqliteDatabaseConnection SqliteConnectionFactory::openReadConnection()
{
    initialize();
    auto connection = openCore(true);
    executePragmas(connection.database(), true);
    return connection;
}

int SqliteConnectionFactory::schemaVersion()
{
    auto connection = openReadConnection();
    QSqlQuery query(connection.database());
    if (!query.exec(QStringLiteral("SELECT MAX(version) FROM schema_migrations;")) || !query.next()) {
        throwSql(QStringLiteral("Failed to read SQLite schema version"), query);
    }

    return query.value(0).toInt();
}

QString SqliteConnectionFactory::journalMode()
{
    auto connection = openReadConnection();
    QSqlQuery query(connection.database());
    if (!query.exec(QStringLiteral("PRAGMA journal_mode;")) || !query.next()) {
        throwSql(QStringLiteral("Failed to read SQLite journal mode"), query);
    }

    return query.value(0).toString();
}

SqliteDatabaseConnection SqliteConnectionFactory::openCore(bool readOnly)
{
    if (!QSqlDatabase::isDriverAvailable(m_driverName)) {
        throw std::runtime_error(QStringLiteral("SQLite driver is not available: %1").arg(m_driverName).toStdString());
    }

    const auto connectionName = createConnectionName();
    auto database = QSqlDatabase::addDatabase(m_driverName, connectionName);
    database.setDatabaseName(m_databasePath);
    database.setConnectOptions(readOnly
        ? QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000;QSQLITE_OPEN_READONLY")
        : QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
    if (!database.open()) {
        const auto error = database.lastError().text();
        database = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
        throw std::runtime_error(QStringLiteral("Failed to open SQLite database: %1").arg(error).toStdString());
    }

    return SqliteDatabaseConnection(connectionName, database);
}

void SqliteConnectionFactory::executePragmas(QSqlDatabase &database, bool readOnly)
{
    execSql(database, QStringLiteral("PRAGMA busy_timeout=5000;"));
    if (readOnly) {
        execSql(database, QStringLiteral("PRAGMA query_only=ON;"));
    } else {
        execSql(database, QStringLiteral("PRAGMA synchronous=NORMAL;"));
        execSql(database, QStringLiteral("PRAGMA foreign_keys=ON;"));
    }
}

} // namespace Monitor::Infrastructure::Persistence
