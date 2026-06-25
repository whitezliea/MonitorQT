#include "SQLiteHistoryRepository.h"

#include "domain/common/DomainCommon.h"

#include <QSqlError>
#include <QSqlQuery>

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

Monitor::Domain::Tags::TagValue readSample(QSqlQuery &query)
{
    return {
        query.value(0).toString(),
        query.value(1).toDouble(),
        Monitor::Domain::Common::UtcDateTime::fromCSharpTicks(query.value(2).toLongLong()),
        static_cast<Monitor::Domain::Tags::TagQuality>(query.value(3).toInt()),
        static_cast<Monitor::Domain::Tags::TagAlarmState>(query.value(4).toInt()),
        query.value(5).toString(),
        query.value(6).toLongLong()
    };
}

} // namespace

SQLiteHistoryRepository::SQLiteHistoryRepository(SqliteConnectionFactory *connectionFactory)
    : m_connectionFactory(connectionFactory)
{
    if (!m_connectionFactory) {
        throw std::invalid_argument("SqliteConnectionFactory must not be null.");
    }
}

void SQLiteHistoryRepository::append(const QVector<Monitor::Domain::Tags::TagValue> &samples)
{
    if (samples.isEmpty()) {
        return;
    }

    for (const auto &sample : samples) {
        Monitor::Domain::Common::UtcDateTime::require(sample.timestampUtc, QStringLiteral("sample.timestampUtc"));
    }

    auto connection = m_connectionFactory->openConnection();
    auto &database = connection.database();
    if (!database.transaction()) {
        throw std::runtime_error(database.lastError().text().toStdString());
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral(R"SQL(
        INSERT OR IGNORE INTO history_samples (
            tag_id, value, timestamp_utc_ticks, quality, alarm_state, source, sequence_no)
        VALUES (
            :tagId, :value, :timestampUtcTicks, :quality, :alarmState, :source, :sequenceNo);
    )SQL"));

    for (const auto &sample : samples) {
        query.bindValue(QStringLiteral(":tagId"), sample.tagId);
        query.bindValue(QStringLiteral(":value"), sample.value);
        query.bindValue(QStringLiteral(":timestampUtcTicks"), ticks(sample.timestampUtc));
        query.bindValue(QStringLiteral(":quality"), static_cast<int>(sample.quality));
        query.bindValue(QStringLiteral(":alarmState"), static_cast<int>(sample.alarmState));
        query.bindValue(QStringLiteral(":source"), sample.source);
        query.bindValue(QStringLiteral(":sequenceNo"), sample.sequenceNo);
        if (!query.exec()) {
            database.rollback();
            throwSql(QStringLiteral("Failed to append history sample"), query);
        }
    }

    if (!database.commit()) {
        throw std::runtime_error(database.lastError().text().toStdString());
    }
}

HistoryQueryResult SQLiteHistoryRepository::query(const HistoryQuery &historyQuery)
{
    historyQuery.validate();

    auto connection = m_connectionFactory->openReadConnection();
    auto &database = connection.database();

    QSqlQuery countQuery(database);
    countQuery.prepare(QStringLiteral(R"SQL(
        SELECT COUNT(*)
        FROM history_samples
        WHERE tag_id = :tagId
          AND timestamp_utc_ticks >= :startTimeUtcTicks
          AND timestamp_utc_ticks <= :endTimeUtcTicks;
    )SQL"));
    countQuery.bindValue(QStringLiteral(":tagId"), historyQuery.tagId);
    countQuery.bindValue(QStringLiteral(":startTimeUtcTicks"), ticks(historyQuery.startTimeUtc));
    countQuery.bindValue(QStringLiteral(":endTimeUtcTicks"), ticks(historyQuery.endTimeUtc));
    if (!countQuery.exec() || !countQuery.next()) {
        throwSql(QStringLiteral("Failed to count history samples"), countQuery);
    }

    const auto direction = historyQuery.sortDirection == HistorySortDirection::Descending
        ? QStringLiteral("DESC")
        : QStringLiteral("ASC");
    QSqlQuery query(database);
    query.prepare(QStringLiteral(R"SQL(
        SELECT tag_id, value, timestamp_utc_ticks, quality, alarm_state, source, sequence_no
        FROM history_samples
        WHERE tag_id = :tagId
          AND timestamp_utc_ticks >= :startTimeUtcTicks
          AND timestamp_utc_ticks <= :endTimeUtcTicks
        ORDER BY timestamp_utc_ticks %1, id %1
        LIMIT :pageSize OFFSET :offset;
    )SQL").arg(direction));
    query.bindValue(QStringLiteral(":tagId"), historyQuery.tagId);
    query.bindValue(QStringLiteral(":startTimeUtcTicks"), ticks(historyQuery.startTimeUtc));
    query.bindValue(QStringLiteral(":endTimeUtcTicks"), ticks(historyQuery.endTimeUtc));
    query.bindValue(QStringLiteral(":pageSize"), historyQuery.pageSize);
    query.bindValue(QStringLiteral(":offset"), (historyQuery.page - 1) * historyQuery.pageSize);
    if (!query.exec()) {
        throwSql(QStringLiteral("Failed to query history samples"), query);
    }

    QVector<Monitor::Domain::Tags::TagValue> samples;
    while (query.next()) {
        samples.append(readSample(query));
    }

    return {samples, countQuery.value(0).toLongLong(), historyQuery.page, historyQuery.pageSize};
}

QVector<Monitor::Domain::Tags::TagValue> SQLiteHistoryRepository::query(
    const QString &tagId,
    const QDateTime &startTimeUtc,
    const QDateTime &endTimeUtc)
{
    return query(HistoryQuery{tagId, startTimeUtc, endTimeUtc, 1, HistoryQuery::MaximumPageSize}).items;
}

int SQLiteHistoryRepository::deleteBefore(const QDateTime &cutoffUtc, int maxRows)
{
    Monitor::Domain::Common::UtcDateTime::require(cutoffUtc, QStringLiteral("cutoffUtc"));
    if (maxRows <= 0) {
        throw std::out_of_range("maxRows must be greater than zero.");
    }

    auto connection = m_connectionFactory->openConnection();
    QSqlQuery query(connection.database());
    query.prepare(QStringLiteral(R"SQL(
        DELETE FROM history_samples
        WHERE id IN (
            SELECT id
            FROM history_samples
            WHERE timestamp_utc_ticks < :cutoffUtcTicks
            ORDER BY timestamp_utc_ticks
            LIMIT :maxRows
        );
    )SQL"));
    query.bindValue(QStringLiteral(":cutoffUtcTicks"), ticks(cutoffUtc));
    query.bindValue(QStringLiteral(":maxRows"), maxRows);
    if (!query.exec()) {
        throwSql(QStringLiteral("Failed to delete retained history samples"), query);
    }

    return query.numRowsAffected();
}

} // namespace Monitor::Infrastructure::Persistence
