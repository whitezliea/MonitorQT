#ifndef QUERYSERVICES_H
#define QUERYSERVICES_H

#include "domain/alarms/AlarmModels.h"
#include "domain/logs/LogModels.h"
#include "domain/tags/TagModels.h"

#include <QDateTime>
#include <QString>
#include <QVector>

#include <functional>
#include <optional>

namespace Monitor::Application::Services {

struct HistoryQueryRequest
{
    QString tagId;
    QDateTime startTimeUtc;
    QDateTime endTimeUtc;
    int page = 1;
    int pageSize = 200;
    bool descending = true;
};

struct HistoryQueryPage
{
    QVector<Monitor::Domain::Tags::TagValue> items;
    qint64 totalCount = 0;
    int page = 1;
    int pageSize = 200;

    bool hasPreviousPage() const { return page > 1; }
    bool hasNextPage() const { return static_cast<qint64>(page) * pageSize < totalCount; }
};

struct AlarmHistoryQueryRequest
{
    QDateTime startTimeUtc;
    QDateTime endTimeUtc;
    std::optional<QString> tagId;
    std::optional<Monitor::Domain::Alarms::AlarmLevel> level;
    std::optional<Monitor::Domain::Alarms::AlarmState> state;
    int page = 1;
    int pageSize = 200;
    bool ascending = false;
};

struct AlarmHistoryQueryPage
{
    QVector<Monitor::Domain::Alarms::AlarmEvent> items;
    qint64 totalCount = 0;
    int page = 1;
    int pageSize = 200;

    bool hasPreviousPage() const { return page > 1; }
    bool hasNextPage() const { return static_cast<qint64>(page) * pageSize < totalCount; }
};

struct OperationLogQueryRequest
{
    QDateTime startTimeUtc;
    QDateTime endTimeUtc;
    std::optional<Monitor::Domain::Logs::OperationLogLevel> level;
    std::optional<QString> categoryText;
    int page = 1;
    int pageSize = 200;
};

struct OperationLogQueryPage
{
    QVector<Monitor::Domain::Logs::OperationLog> items;
    qint64 totalCount = 0;
    int page = 1;
    int pageSize = 200;

    bool hasPreviousPage() const { return page > 1; }
    bool hasNextPage() const { return static_cast<qint64>(page) * pageSize < totalCount; }
};

class HistoryQueryService
{
public:
    using QueryFunction = std::function<HistoryQueryPage(const HistoryQueryRequest &)>;

    explicit HistoryQueryService(QueryFunction query);
    HistoryQueryPage query(const HistoryQueryRequest &request) const;

private:
    QueryFunction m_query;
};

class AlarmQueryService
{
public:
    using QueryFunction = std::function<AlarmHistoryQueryPage(const AlarmHistoryQueryRequest &)>;

    explicit AlarmQueryService(QueryFunction query);
    AlarmHistoryQueryPage query(const AlarmHistoryQueryRequest &request) const;

private:
    QueryFunction m_query;
};

class OperationLogQueryService
{
public:
    using QueryFunction = std::function<OperationLogQueryPage(const OperationLogQueryRequest &)>;

    explicit OperationLogQueryService(QueryFunction query);
    OperationLogQueryPage query(const OperationLogQueryRequest &request) const;

private:
    QueryFunction m_query;
};

} // namespace Monitor::Application::Services

#endif // QUERYSERVICES_H
