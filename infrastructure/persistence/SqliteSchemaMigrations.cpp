#include "SqliteSchemaMigrations.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace Monitor::Infrastructure::Persistence {
namespace {

void throwSql(const QString &message, const QSqlQuery &query)
{
    throw std::runtime_error(QStringLiteral("%1: %2").arg(message, query.lastError().text()).toStdString());
}

void throwSql(const QString &message, const QSqlDatabase &database)
{
    throw std::runtime_error(QStringLiteral("%1: %2").arg(message, database.lastError().text()).toStdString());
}

} // namespace

QVector<SqliteSchemaMigration> SqliteSchemaMigrations::all()
{
    return {
        {1, QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS history_samples (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                tag_id TEXT NOT NULL,
                value REAL NOT NULL,
                timestamp_utc_ticks INTEGER NOT NULL,
                quality INTEGER NOT NULL,
                alarm_state INTEGER NOT NULL,
                source TEXT NOT NULL,
                sequence_no INTEGER NOT NULL
            );

            CREATE INDEX IF NOT EXISTS idx_history_samples_tag_time
            ON history_samples(tag_id, timestamp_utc_ticks);

            CREATE TABLE IF NOT EXISTS alarm_events (
                alarm_id TEXT PRIMARY KEY,
                tag_id TEXT NOT NULL,
                level INTEGER NOT NULL,
                state INTEGER NOT NULL,
                trigger_value REAL NOT NULL,
                trigger_time_utc_ticks INTEGER NOT NULL,
                message TEXT NOT NULL,
                acknowledge_time_utc_ticks INTEGER NULL,
                recover_time_utc_ticks INTEGER NULL
            );

            CREATE INDEX IF NOT EXISTS idx_alarm_events_trigger_time
            ON alarm_events(trigger_time_utc_ticks DESC);

            CREATE INDEX IF NOT EXISTS idx_alarm_events_tag_time
            ON alarm_events(tag_id, trigger_time_utc_ticks DESC);
        )SQL")},
        {2, QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS operation_logs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp_utc_ticks INTEGER NOT NULL,
                level INTEGER NOT NULL,
                category TEXT NOT NULL,
                action TEXT NOT NULL,
                source TEXT NOT NULL,
                message TEXT NOT NULL,
                detail TEXT NULL,
                correlation_id TEXT NULL
            );

            CREATE INDEX IF NOT EXISTS idx_operation_logs_time
            ON operation_logs(timestamp_utc_ticks DESC);

            CREATE INDEX IF NOT EXISTS idx_operation_logs_category_time
            ON operation_logs(category, timestamp_utc_ticks DESC);
        )SQL")},
        {3, QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS tag_runtime_settings (
                tag_id TEXT PRIMARY KEY,
                alarm_enabled INTEGER NOT NULL,
                warning_low REAL NULL,
                alarm_low REAL NULL,
                warning_high REAL NULL,
                alarm_high REAL NULL,
                is_historized INTEGER NOT NULL,
                history_interval_ms INTEGER NOT NULL
            );

            CREATE TABLE IF NOT EXISTS runtime_settings (
                setting_key TEXT PRIMARY KEY,
                setting_value TEXT NOT NULL
            );
        )SQL")},
        {4, QStringLiteral(R"SQL(
            DELETE FROM history_samples
            WHERE id NOT IN (
                SELECT MIN(id)
                FROM history_samples
                GROUP BY tag_id, source
            );

            CREATE UNIQUE INDEX IF NOT EXISTS idx_history_samples_tag_source
            ON history_samples(tag_id, source);
        )SQL")},
        {5, QStringLiteral(R"SQL(
            ALTER TABLE alarm_events ADD COLUMN alarm_type INTEGER NOT NULL DEFAULT 5;
            ALTER TABLE alarm_events ADD COLUMN last_updated_time_utc_ticks INTEGER NULL;
            ALTER TABLE alarm_events ADD COLUMN close_reason TEXT NULL;

            UPDATE alarm_events
            SET last_updated_time_utc_ticks = COALESCE(
                recover_time_utc_ticks,
                acknowledge_time_utc_ticks,
                trigger_time_utc_ticks)
            WHERE last_updated_time_utc_ticks IS NULL;

            CREATE INDEX IF NOT EXISTS idx_alarm_events_state_time
            ON alarm_events(state, trigger_time_utc_ticks DESC);
        )SQL")}
    };
}

int SqliteSchemaMigrations::currentVersion()
{
    return 5;
}

SqliteSchemaMigrator::SqliteSchemaMigrator(QVector<SqliteSchemaMigration> migrations)
    : m_migrations(std::move(migrations))
{
    std::sort(m_migrations.begin(), m_migrations.end(), [](const auto &left, const auto &right) {
        return left.version < right.version;
    });
    validateMigrationDefinitions(m_migrations);
}

int SqliteSchemaMigrator::targetVersion() const
{
    return m_migrations.isEmpty() ? 0 : m_migrations.last().version;
}

int SqliteSchemaMigrator::migrate(QSqlDatabase &database) const
{
    ensureMigrationTable(database);
    const auto versions = appliedVersions(database);
    validateAppliedVersions(versions);

    auto currentVersion = versions.isEmpty() ? 0 : versions.last();
    for (const auto &migration : m_migrations) {
        if (migration.version <= currentVersion) {
            continue;
        }

        applyMigration(database, migration);
        currentVersion = migration.version;
    }

    return currentVersion;
}

void SqliteSchemaMigrator::validateMigrationDefinitions(const QVector<SqliteSchemaMigration> &migrations)
{
    for (auto index = 0; index < migrations.size(); ++index) {
        const auto expectedVersion = index + 1;
        if (migrations.at(index).version != expectedVersion) {
            throw std::runtime_error("SQLite migrations must be contiguous and start at version 1.");
        }

        if (migrations.at(index).sql.trimmed().isEmpty()) {
            throw std::runtime_error("SQLite migration SQL must not be empty.");
        }
    }
}

void SqliteSchemaMigrator::ensureMigrationTable(QSqlDatabase &database)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(R"SQL(
        CREATE TABLE IF NOT EXISTS schema_migrations (
            version INTEGER PRIMARY KEY,
            applied_at_utc TEXT NOT NULL
        );
    )SQL"))) {
        throwSql(QStringLiteral("Failed to create schema_migrations"), query);
    }
}

QVector<int> SqliteSchemaMigrator::appliedVersions(QSqlDatabase &database)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT version FROM schema_migrations ORDER BY version;"))) {
        throwSql(QStringLiteral("Failed to read schema migrations"), query);
    }

    QVector<int> versions;
    while (query.next()) {
        versions.append(query.value(0).toInt());
    }
    return versions;
}

void SqliteSchemaMigrator::validateAppliedVersions(const QVector<int> &versions) const
{
    for (auto index = 0; index < versions.size(); ++index) {
        if (versions.at(index) != index + 1) {
            throw std::runtime_error("SQLite schema migration history is not contiguous.");
        }
    }

    if (!versions.isEmpty() && versions.last() > targetVersion()) {
        throw std::runtime_error("SQLite database schema is newer than the supported version.");
    }
}

void SqliteSchemaMigrator::applyMigration(QSqlDatabase &database, const SqliteSchemaMigration &migration)
{
    if (!database.transaction()) {
        throwSql(QStringLiteral("Failed to begin SQLite migration transaction"), database);
    }

    try {
        executeSqlBatch(database, migration.sql);

        QSqlQuery history(database);
        history.prepare(QStringLiteral(
            "INSERT INTO schema_migrations (version, applied_at_utc) VALUES (:version, :appliedAtUtc);"));
        history.bindValue(QStringLiteral(":version"), migration.version);
        history.bindValue(QStringLiteral(":appliedAtUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        if (!history.exec()) {
            throwSql(QStringLiteral("Failed to record SQLite migration"), history);
        }

        if (!database.commit()) {
            throwSql(QStringLiteral("Failed to commit SQLite migration"), database);
        }
    } catch (...) {
        database.rollback();
        throw;
    }
}

void SqliteSchemaMigrator::executeSqlBatch(QSqlDatabase &database, const QString &sql)
{
    const auto statements = sql.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const auto &statement : statements) {
        const auto trimmed = statement.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        QSqlQuery query(database);
        if (!query.exec(trimmed)) {
            throwSql(QStringLiteral("Failed to execute SQLite migration statement"), query);
        }
    }
}

} // namespace Monitor::Infrastructure::Persistence
