#include "SQLiteConfigurationRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <stdexcept>

namespace Monitor::Infrastructure::Persistence {
namespace {

void throwSql(const QString &message, const QSqlQuery &query)
{
    throw std::runtime_error(QStringLiteral("%1: %2").arg(message, query.lastError().text()).toStdString());
}

QVariant optionalDouble(const std::optional<double> &value)
{
    return value.has_value() ? QVariant(value.value()) : QVariant();
}

std::optional<double> readOptionalDouble(const QSqlQuery &query, int index)
{
    return query.value(index).isNull() ? std::optional<double>() : query.value(index).toDouble();
}

} // namespace

SQLiteConfigurationRepository::SQLiteConfigurationRepository(SqliteConnectionFactory *connectionFactory)
    : m_connectionFactory(connectionFactory)
{
    if (!m_connectionFactory) {
        throw std::invalid_argument("SqliteConnectionFactory must not be null.");
    }
}

QVector<Monitor::Application::Configuration::TagRuntimeConfiguration>
SQLiteConfigurationRepository::loadTagConfigurations()
{
    auto connection = m_connectionFactory->openReadConnection();
    QSqlQuery query(connection.database());
    if (!query.exec(QStringLiteral(R"SQL(
        SELECT tag_id, alarm_enabled, warning_low, alarm_low,
               warning_high, alarm_high, is_historized, history_interval_ms
        FROM tag_runtime_settings;
    )SQL"))) {
        throwSql(QStringLiteral("Failed to load tag runtime configurations"), query);
    }

    QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> configurations;
    while (query.next()) {
        configurations.append({
            query.value(0).toString(),
            query.value(1).toInt() != 0,
            readOptionalDouble(query, 2),
            readOptionalDouble(query, 3),
            readOptionalDouble(query, 4),
            readOptionalDouble(query, 5),
            query.value(6).toInt() != 0,
            query.value(7).toInt(),
            0
        });
    }

    return configurations;
}

void SQLiteConfigurationRepository::saveTagConfigurations(
    const QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> &configurations)
{
    auto connection = m_connectionFactory->openConnection();
    auto &database = connection.database();
    if (!database.transaction()) {
        throw std::runtime_error(database.lastError().text().toStdString());
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral(R"SQL(
        INSERT INTO tag_runtime_settings (
            tag_id, alarm_enabled, warning_low, alarm_low,
            warning_high, alarm_high, is_historized, history_interval_ms)
        VALUES (
            :tagId, :alarmEnabled, :warningLow, :alarmLow,
            :warningHigh, :alarmHigh, :isHistorized, :historyIntervalMs)
        ON CONFLICT(tag_id) DO UPDATE SET
            alarm_enabled = excluded.alarm_enabled,
            warning_low = excluded.warning_low,
            alarm_low = excluded.alarm_low,
            warning_high = excluded.warning_high,
            alarm_high = excluded.alarm_high,
            is_historized = excluded.is_historized,
            history_interval_ms = excluded.history_interval_ms;
    )SQL"));

    for (const auto &configuration : configurations) {
        if (configuration.tagId.trimmed().isEmpty()) {
            database.rollback();
            throw std::invalid_argument("Tag runtime configuration tagId cannot be empty.");
        }
        if (configuration.historyIntervalMs <= 0) {
            database.rollback();
            throw std::out_of_range("Tag runtime configuration historyIntervalMs must be greater than zero.");
        }

        query.bindValue(QStringLiteral(":tagId"), configuration.tagId);
        query.bindValue(QStringLiteral(":alarmEnabled"), configuration.alarmEnabled ? 1 : 0);
        query.bindValue(QStringLiteral(":warningLow"), optionalDouble(configuration.warningLow));
        query.bindValue(QStringLiteral(":alarmLow"), optionalDouble(configuration.alarmLow));
        query.bindValue(QStringLiteral(":warningHigh"), optionalDouble(configuration.warningHigh));
        query.bindValue(QStringLiteral(":alarmHigh"), optionalDouble(configuration.alarmHigh));
        query.bindValue(QStringLiteral(":isHistorized"), configuration.isHistorized ? 1 : 0);
        query.bindValue(QStringLiteral(":historyIntervalMs"), configuration.historyIntervalMs);
        if (!query.exec()) {
            database.rollback();
            throwSql(QStringLiteral("Failed to save tag runtime configuration"), query);
        }
    }

    if (!database.commit()) {
        throw std::runtime_error(database.lastError().text().toStdString());
    }
}

QHash<QString, QString> SQLiteConfigurationRepository::loadRuntimeSettings()
{
    auto connection = m_connectionFactory->openReadConnection();
    QSqlQuery query(connection.database());
    if (!query.exec(QStringLiteral("SELECT setting_key, setting_value FROM runtime_settings;"))) {
        throwSql(QStringLiteral("Failed to load runtime settings"), query);
    }

    QHash<QString, QString> settings;
    while (query.next()) {
        settings.insert(query.value(0).toString(), query.value(1).toString());
    }
    return settings;
}

void SQLiteConfigurationRepository::saveRuntimeSettings(const QHash<QString, QString> &settings)
{
    auto connection = m_connectionFactory->openConnection();
    auto &database = connection.database();
    if (!database.transaction()) {
        throw std::runtime_error(database.lastError().text().toStdString());
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral(R"SQL(
        INSERT INTO runtime_settings (setting_key, setting_value)
        VALUES (:key, :value)
        ON CONFLICT(setting_key) DO UPDATE SET setting_value = excluded.setting_value;
    )SQL"));

    for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
        if (it.key().trimmed().isEmpty()) {
            database.rollback();
            throw std::invalid_argument("Runtime setting key cannot be empty.");
        }

        query.bindValue(QStringLiteral(":key"), it.key());
        query.bindValue(QStringLiteral(":value"), it.value());
        if (!query.exec()) {
            database.rollback();
            throwSql(QStringLiteral("Failed to save runtime setting"), query);
        }
    }

    if (!database.commit()) {
        throw std::runtime_error(database.lastError().text().toStdString());
    }
}

} // namespace Monitor::Infrastructure::Persistence
