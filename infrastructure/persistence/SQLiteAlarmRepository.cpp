#include "SQLiteAlarmRepository.h"

#include "domain/common/DomainCommon.h"

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

qint64 ticks(const QDateTime &timestampUtc)
{
    return Monitor::Domain::Common::UtcDateTime::toCSharpTicks(timestampUtc);
}

QVariant optionalTicks(const std::optional<QDateTime> &timestampUtc)
{
    return timestampUtc.has_value()
        ? QVariant(ticks(timestampUtc.value()))
        : QVariant();
}

QVariant optionalString(const std::optional<QString> &value)
{
    return value.has_value() ? QVariant(value.value()) : QVariant();
}

QUuid parseUuid(const QString &value)
{
    const auto withBraces = value.startsWith(QLatin1Char('{'))
        ? value
        : QStringLiteral("{%1}").arg(value);
    return QUuid(withBraces);
}

Monitor::Domain::Alarms::AlarmEvent readAlarm(QSqlQuery &query)
{
    Monitor::Domain::Alarms::AlarmEvent alarm;
    alarm.alarmId = parseUuid(query.value(0).toString());
    alarm.tagId = query.value(1).toString();
    alarm.level = static_cast<Monitor::Domain::Alarms::AlarmLevel>(query.value(2).toInt());
    alarm.state = static_cast<Monitor::Domain::Alarms::AlarmState>(query.value(3).toInt());
    alarm.triggerValue = query.value(4).toDouble();
    alarm.triggerTimeUtc = Monitor::Domain::Common::UtcDateTime::fromCSharpTicks(query.value(5).toLongLong());
    alarm.message = query.value(6).toString();
    if (!query.value(7).isNull()) {
        alarm.acknowledgeTimeUtc = Monitor::Domain::Common::UtcDateTime::fromCSharpTicks(query.value(7).toLongLong());
    }
    if (!query.value(8).isNull()) {
        alarm.recoverTimeUtc = Monitor::Domain::Common::UtcDateTime::fromCSharpTicks(query.value(8).toLongLong());
    }
    alarm.alarmType = static_cast<Monitor::Domain::Tags::TagAlarmState>(query.value(9).toInt());
    if (!query.value(10).isNull()) {
        alarm.lastUpdatedTimeUtc = Monitor::Domain::Common::UtcDateTime::fromCSharpTicks(query.value(10).toLongLong());
    }
    if (!query.value(11).isNull()) {
        alarm.closeReason = query.value(11).toString();
    }
    return alarm;
}

QString selectColumns()
{
    return QStringLiteral(R"SQL(
        SELECT alarm_id, tag_id, level, state, trigger_value,
               trigger_time_utc_ticks, message, acknowledge_time_utc_ticks,
               recover_time_utc_ticks, alarm_type, last_updated_time_utc_ticks,
               close_reason
    )SQL");
}

void bindAlarmQuery(QSqlQuery &sqlQuery, const AlarmQuery &query)
{
    sqlQuery.bindValue(QStringLiteral(":startUtcTicks"), ticks(query.startTimeUtc));
    sqlQuery.bindValue(QStringLiteral(":endUtcTicks"), ticks(query.endTimeUtc));
    sqlQuery.bindValue(QStringLiteral(":tagId"), query.tagId.has_value() && !query.tagId->trimmed().isEmpty() ? QVariant(query.tagId->trimmed()) : QVariant());
    sqlQuery.bindValue(QStringLiteral(":level"), query.level.has_value() ? QVariant(static_cast<int>(query.level.value())) : QVariant());
    sqlQuery.bindValue(QStringLiteral(":state"), query.state.has_value() ? QVariant(static_cast<int>(query.state.value())) : QVariant());
}

QVector<Monitor::Domain::Alarms::AlarmEvent> readAlarms(QSqlQuery &query)
{
    QVector<Monitor::Domain::Alarms::AlarmEvent> alarms;
    while (query.next()) {
        alarms.append(readAlarm(query));
    }
    return alarms;
}

} // namespace

SQLiteAlarmRepository::SQLiteAlarmRepository(SqliteConnectionFactory *connectionFactory)
    : m_connectionFactory(connectionFactory)
{
    if (!m_connectionFactory) {
        throw std::invalid_argument("SqliteConnectionFactory must not be null.");
    }
}

void SQLiteAlarmRepository::append(const QVector<Monitor::Domain::Alarms::AlarmEvent> &alarms)
{
    if (alarms.isEmpty()) {
        return;
    }

    for (const auto &alarm : alarms) {
        Monitor::Domain::Common::UtcDateTime::require(alarm.triggerTimeUtc, QStringLiteral("alarm.triggerTimeUtc"));
        Monitor::Domain::Common::UtcDateTime::require(alarm.acknowledgeTimeUtc, QStringLiteral("alarm.acknowledgeTimeUtc"));
        Monitor::Domain::Common::UtcDateTime::require(alarm.recoverTimeUtc, QStringLiteral("alarm.recoverTimeUtc"));
        Monitor::Domain::Common::UtcDateTime::require(alarm.lastUpdatedTimeUtc, QStringLiteral("alarm.lastUpdatedTimeUtc"));
    }

    auto connection = m_connectionFactory->openConnection();
    auto &database = connection.database();
    if (!database.transaction()) {
        throw std::runtime_error(database.lastError().text().toStdString());
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral(R"SQL(
        INSERT INTO alarm_events (
            alarm_id, tag_id, level, state, trigger_value, trigger_time_utc_ticks,
            message, acknowledge_time_utc_ticks, recover_time_utc_ticks,
            alarm_type, last_updated_time_utc_ticks, close_reason)
        VALUES (
            :alarmId, :tagId, :level, :state, :triggerValue, :triggerTimeUtcTicks,
            :message, :acknowledgeTimeUtcTicks, :recoverTimeUtcTicks,
            :alarmType, :lastUpdatedTimeUtcTicks, :closeReason)
        ON CONFLICT(alarm_id) DO UPDATE SET
            tag_id = excluded.tag_id,
            level = excluded.level,
            state = excluded.state,
            trigger_value = excluded.trigger_value,
            trigger_time_utc_ticks = excluded.trigger_time_utc_ticks,
            message = excluded.message,
            acknowledge_time_utc_ticks = excluded.acknowledge_time_utc_ticks,
            recover_time_utc_ticks = excluded.recover_time_utc_ticks,
            alarm_type = excluded.alarm_type,
            last_updated_time_utc_ticks = excluded.last_updated_time_utc_ticks,
            close_reason = excluded.close_reason;
    )SQL"));

    for (const auto &alarm : alarms) {
        query.bindValue(QStringLiteral(":alarmId"), alarm.alarmId.toString(QUuid::WithoutBraces));
        query.bindValue(QStringLiteral(":tagId"), alarm.tagId);
        query.bindValue(QStringLiteral(":level"), static_cast<int>(alarm.level));
        query.bindValue(QStringLiteral(":state"), static_cast<int>(alarm.state));
        query.bindValue(QStringLiteral(":triggerValue"), alarm.triggerValue);
        query.bindValue(QStringLiteral(":triggerTimeUtcTicks"), ticks(alarm.triggerTimeUtc));
        query.bindValue(QStringLiteral(":message"), alarm.message);
        query.bindValue(QStringLiteral(":acknowledgeTimeUtcTicks"), optionalTicks(alarm.acknowledgeTimeUtc));
        query.bindValue(QStringLiteral(":recoverTimeUtcTicks"), optionalTicks(alarm.recoverTimeUtc));
        query.bindValue(QStringLiteral(":alarmType"), static_cast<int>(alarm.alarmType));
        query.bindValue(QStringLiteral(":lastUpdatedTimeUtcTicks"), optionalTicks(alarm.lastUpdatedTimeUtc.has_value()
                ? alarm.lastUpdatedTimeUtc
                : std::optional<QDateTime>(alarm.triggerTimeUtc)));
        query.bindValue(QStringLiteral(":closeReason"), optionalString(alarm.closeReason));
        if (!query.exec()) {
            database.rollback();
            throwSql(QStringLiteral("Failed to append alarm event"), query);
        }
    }

    if (!database.commit()) {
        throw std::runtime_error(database.lastError().text().toStdString());
    }
}

QVector<Monitor::Domain::Alarms::AlarmEvent> SQLiteAlarmRepository::queryLatest(int count)
{
    if (count <= 0) {
        return {};
    }

    auto connection = m_connectionFactory->openReadConnection();
    QSqlQuery query(connection.database());
    query.prepare(selectColumns() + QStringLiteral(R"SQL(
        FROM alarm_events
        ORDER BY trigger_time_utc_ticks DESC
        LIMIT :count;
    )SQL"));
    query.bindValue(QStringLiteral(":count"), count);
    if (!query.exec()) {
        throwSql(QStringLiteral("Failed to query latest alarms"), query);
    }
    return readAlarms(query);
}

QVector<Monitor::Domain::Alarms::AlarmEvent> SQLiteAlarmRepository::queryOpenAlarms()
{
    auto connection = m_connectionFactory->openReadConnection();
    QSqlQuery query(connection.database());
    query.prepare(selectColumns() + QStringLiteral(R"SQL(
        FROM alarm_events
        WHERE state = :active OR state = :acknowledged
        ORDER BY trigger_time_utc_ticks DESC;
    )SQL"));
    query.bindValue(QStringLiteral(":active"), static_cast<int>(Monitor::Domain::Alarms::AlarmState::Active));
    query.bindValue(QStringLiteral(":acknowledged"), static_cast<int>(Monitor::Domain::Alarms::AlarmState::Acknowledged));
    if (!query.exec()) {
        throwSql(QStringLiteral("Failed to query open alarms"), query);
    }
    return readAlarms(query);
}

AlarmQueryResult SQLiteAlarmRepository::query(const AlarmQuery &alarmQuery)
{
    alarmQuery.validate();

    auto connection = m_connectionFactory->openReadConnection();
    auto &database = connection.database();
    const auto where = QStringLiteral(R"SQL(
        WHERE trigger_time_utc_ticks >= :startUtcTicks
          AND trigger_time_utc_ticks <= :endUtcTicks
          AND (:tagId IS NULL OR tag_id = :tagId)
          AND (:level IS NULL OR level = :level)
          AND (:state IS NULL OR state = :state)
    )SQL");

    QSqlQuery countQuery(database);
    countQuery.prepare(QStringLiteral("SELECT COUNT(*) FROM alarm_events %1;").arg(where));
    bindAlarmQuery(countQuery, alarmQuery);
    if (!countQuery.exec() || !countQuery.next()) {
        throwSql(QStringLiteral("Failed to count alarms"), countQuery);
    }

    const auto direction = alarmQuery.sortDirection == AlarmSortDirection::Ascending
        ? QStringLiteral("ASC")
        : QStringLiteral("DESC");
    QSqlQuery query(database);
    query.prepare(selectColumns() + QStringLiteral(R"SQL(
        FROM alarm_events
        %1
        ORDER BY trigger_time_utc_ticks %2, alarm_id %2
        LIMIT :pageSize OFFSET :offset;
    )SQL").arg(where, direction));
    bindAlarmQuery(query, alarmQuery);
    query.bindValue(QStringLiteral(":pageSize"), alarmQuery.pageSize);
    query.bindValue(QStringLiteral(":offset"), (alarmQuery.page - 1) * alarmQuery.pageSize);
    if (!query.exec()) {
        throwSql(QStringLiteral("Failed to query alarms"), query);
    }

    return {readAlarms(query), countQuery.value(0).toLongLong(), alarmQuery.page, alarmQuery.pageSize};
}

} // namespace Monitor::Infrastructure::Persistence
