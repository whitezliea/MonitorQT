#include "SQLiteOperationLogRepository.h"

#include "domain/common/DomainCommon.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <stdexcept>

namespace Monitor::Infrastructure::Persistence {
namespace {

constexpr int MaximumOperationLogCount = 5000;
constexpr int MaximumOperationLogPageSize = 1000;

void throwSql(const QString &message, const QSqlQuery &query)
{
    throw std::runtime_error(QStringLiteral("%1: %2").arg(message, query.lastError().text()).toStdString());
}

void throwArgument(const QString &message)
{
    throw std::invalid_argument(message.toStdString());
}

qint64 ticks(const QDateTime &timestampUtc)
{
    return Monitor::Domain::Common::UtcDateTime::toCSharpTicks(timestampUtc);
}

QVariant optionalString(const std::optional<QString> &value)
{
    return value.has_value() ? QVariant(value.value()) : QVariant();
}

Monitor::Domain::Logs::OperationLog readLog(QSqlQuery &query)
{
    return {
        Monitor::Domain::Common::UtcDateTime::fromCSharpTicks(query.value(1).toLongLong()),
        static_cast<Monitor::Domain::Logs::OperationLogLevel>(query.value(2).toInt()),
        query.value(3).toString(),
        query.value(6).toString(),
        query.value(4).toString(),
        query.value(5).toString(),
        query.value(7).isNull() ? std::optional<QString>() : query.value(7).toString(),
        query.value(8).isNull() ? std::optional<QString>() : query.value(8).toString(),
        query.value(0).toLongLong()
    };
}

QString selectColumns()
{
    return QStringLiteral(R"SQL(
        SELECT id, timestamp_utc_ticks, level, category, action, source,
               message, detail, correlation_id
    )SQL");
}

void validate(const Monitor::Domain::Logs::OperationLog &log)
{
    Monitor::Domain::Common::UtcDateTime::require(log.timestampUtc, QStringLiteral("log.timestampUtc"));
    if (log.category.trimmed().isEmpty()) {
        throwArgument(QStringLiteral("Operation log category cannot be empty."));
    }
    if (log.action.trimmed().isEmpty()) {
        throwArgument(QStringLiteral("Operation log action cannot be empty."));
    }
    if (log.source.trimmed().isEmpty()) {
        throwArgument(QStringLiteral("Operation log source cannot be empty."));
    }
    if (log.message.trimmed().isEmpty()) {
        throwArgument(QStringLiteral("Operation log message cannot be empty."));
    }
}

void validate(const Monitor::Domain::Logs::OperationLogQuery &query)
{
    Monitor::Domain::Common::UtcDateTime::require(query.startTimeUtc, QStringLiteral("operationLogQuery.startTimeUtc"));
    Monitor::Domain::Common::UtcDateTime::require(query.endTimeUtc, QStringLiteral("operationLogQuery.endTimeUtc"));
    if (query.startTimeUtc > query.endTimeUtc) {
        throwArgument(QStringLiteral("Operation log start time must not be later than end time."));
    }
    if (query.maxCount < 0 || query.maxCount > MaximumOperationLogCount) {
        throw std::out_of_range("Operation log maxCount must be between 0 and 5000.");
    }
    if (query.page <= 0) {
        throw std::out_of_range("Operation log page must be greater than zero.");
    }
    if (query.pageSize <= 0 || query.pageSize > MaximumOperationLogPageSize) {
        throw std::out_of_range("Operation log pageSize is out of range.");
    }
}

QVector<Monitor::Domain::Logs::OperationLog> readLogs(QSqlQuery &query)
{
    QVector<Monitor::Domain::Logs::OperationLog> logs;
    while (query.next()) {
        logs.append(readLog(query));
    }
    return logs;
}

} // namespace

SQLiteOperationLogRepository::SQLiteOperationLogRepository(SqliteConnectionFactory *connectionFactory)
    : m_connectionFactory(connectionFactory)
{
    if (!m_connectionFactory) {
        throw std::invalid_argument("SqliteConnectionFactory must not be null.");
    }
}

void SQLiteOperationLogRepository::append(const QVector<Monitor::Domain::Logs::OperationLog> &logs)
{
    if (logs.isEmpty()) {
        return;
    }

    for (const auto &log : logs) {
        validate(log);
    }

    auto connection = m_connectionFactory->openConnection();
    auto &database = connection.database();
    if (!database.transaction()) {
        throw std::runtime_error(database.lastError().text().toStdString());
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral(R"SQL(
        INSERT INTO operation_logs (
            timestamp_utc_ticks, level, category, action, source,
            message, detail, correlation_id)
        VALUES (
            :timestampUtcTicks, :level, :category, :action, :source,
            :message, :detail, :correlationId);
    )SQL"));

    for (const auto &log : logs) {
        query.bindValue(QStringLiteral(":timestampUtcTicks"), ticks(log.timestampUtc));
        query.bindValue(QStringLiteral(":level"), static_cast<int>(log.level));
        query.bindValue(QStringLiteral(":category"), log.category);
        query.bindValue(QStringLiteral(":action"), log.action);
        query.bindValue(QStringLiteral(":source"), log.source);
        query.bindValue(QStringLiteral(":message"), log.message);
        query.bindValue(QStringLiteral(":detail"), optionalString(log.detail));
        query.bindValue(QStringLiteral(":correlationId"), optionalString(log.correlationId));
        if (!query.exec()) {
            database.rollback();
            throwSql(QStringLiteral("Failed to append operation log"), query);
        }
    }

    if (!database.commit()) {
        throw std::runtime_error(database.lastError().text().toStdString());
    }
}

QVector<Monitor::Domain::Logs::OperationLog> SQLiteOperationLogRepository::queryLatest(int count)
{
    if (count <= 0) {
        return {};
    }
    if (count > MaximumOperationLogCount) {
        throw std::out_of_range("Operation log count must be between 0 and 5000.");
    }

    auto connection = m_connectionFactory->openReadConnection();
    QSqlQuery query(connection.database());
    query.prepare(selectColumns() + QStringLiteral(R"SQL(
        FROM operation_logs
        ORDER BY timestamp_utc_ticks DESC, id DESC
        LIMIT :count;
    )SQL"));
    query.bindValue(QStringLiteral(":count"), count);
    if (!query.exec()) {
        throwSql(QStringLiteral("Failed to query latest operation logs"), query);
    }

    return readLogs(query);
}

QVector<Monitor::Domain::Logs::OperationLog> SQLiteOperationLogRepository::query(
    const Monitor::Domain::Logs::OperationLogQuery &operationLogQuery)
{
    validate(operationLogQuery);
    if (operationLogQuery.maxCount == 0) {
        return {};
    }

    auto connection = m_connectionFactory->openReadConnection();
    QSqlQuery query(connection.database());
    query.prepare(selectColumns() + QStringLiteral(R"SQL(
        FROM operation_logs
        WHERE timestamp_utc_ticks >= :startTimeUtcTicks
          AND timestamp_utc_ticks <= :endTimeUtcTicks
          AND (:level IS NULL OR level = :level)
          AND (:category = '' OR category = :category COLLATE NOCASE)
        ORDER BY timestamp_utc_ticks DESC, id DESC
        LIMIT :maxCount;
    )SQL"));
    query.bindValue(QStringLiteral(":startTimeUtcTicks"), ticks(operationLogQuery.startTimeUtc));
    query.bindValue(QStringLiteral(":endTimeUtcTicks"), ticks(operationLogQuery.endTimeUtc));
    query.bindValue(QStringLiteral(":level"), operationLogQuery.level.has_value()
            ? QVariant(static_cast<int>(operationLogQuery.level.value()))
            : QVariant());
    query.bindValue(QStringLiteral(":category"), operationLogQuery.category.has_value()
            ? operationLogQuery.category->trimmed()
            : QString());
    query.bindValue(QStringLiteral(":maxCount"), operationLogQuery.maxCount);
    if (!query.exec()) {
        throwSql(QStringLiteral("Failed to query operation logs"), query);
    }

    return readLogs(query);
}

Monitor::Domain::Logs::OperationLogQueryResult SQLiteOperationLogRepository::queryPage(
    const Monitor::Domain::Logs::OperationLogQuery &operationLogQuery)
{
    validate(operationLogQuery);

    auto connection = m_connectionFactory->openReadConnection();
    auto &database = connection.database();
    const auto where = QStringLiteral(R"SQL(
        WHERE timestamp_utc_ticks >= :startTimeUtcTicks
          AND timestamp_utc_ticks <= :endTimeUtcTicks
          AND (:level IS NULL OR level = :level)
          AND (:category = '' OR category = :category COLLATE NOCASE)
    )SQL");

    QSqlQuery countQuery(database);
    countQuery.prepare(QStringLiteral("SELECT COUNT(*) FROM operation_logs %1;").arg(where));
    countQuery.bindValue(QStringLiteral(":startTimeUtcTicks"), ticks(operationLogQuery.startTimeUtc));
    countQuery.bindValue(QStringLiteral(":endTimeUtcTicks"), ticks(operationLogQuery.endTimeUtc));
    countQuery.bindValue(QStringLiteral(":level"), operationLogQuery.level.has_value()
            ? QVariant(static_cast<int>(operationLogQuery.level.value()))
            : QVariant());
    countQuery.bindValue(QStringLiteral(":category"), operationLogQuery.category.has_value()
            ? operationLogQuery.category->trimmed()
            : QString());
    if (!countQuery.exec() || !countQuery.next()) {
        throwSql(QStringLiteral("Failed to count operation logs"), countQuery);
    }

    QSqlQuery query(database);
    query.prepare(selectColumns() + QStringLiteral(R"SQL(
        FROM operation_logs
        %1
        ORDER BY timestamp_utc_ticks DESC, id DESC
        LIMIT :pageSize OFFSET :offset;
    )SQL").arg(where));
    query.bindValue(QStringLiteral(":startTimeUtcTicks"), ticks(operationLogQuery.startTimeUtc));
    query.bindValue(QStringLiteral(":endTimeUtcTicks"), ticks(operationLogQuery.endTimeUtc));
    query.bindValue(QStringLiteral(":level"), operationLogQuery.level.has_value()
            ? QVariant(static_cast<int>(operationLogQuery.level.value()))
            : QVariant());
    query.bindValue(QStringLiteral(":category"), operationLogQuery.category.has_value()
            ? operationLogQuery.category->trimmed()
            : QString());
    query.bindValue(QStringLiteral(":pageSize"), operationLogQuery.pageSize);
    query.bindValue(QStringLiteral(":offset"), (operationLogQuery.page - 1) * operationLogQuery.pageSize);
    if (!query.exec()) {
        throwSql(QStringLiteral("Failed to query operation log page"), query);
    }

    return {
        readLogs(query),
        countQuery.value(0).toLongLong(),
        operationLogQuery.page,
        operationLogQuery.pageSize
    };
}

} // namespace Monitor::Infrastructure::Persistence
