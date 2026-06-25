#ifndef PERSISTENCEMODELS_H
#define PERSISTENCEMODELS_H

#include "domain/alarms/AlarmModels.h"
#include "domain/common/DomainCommon.h"
#include "domain/tags/TagModels.h"

#include <QDateTime>
#include <QString>
#include <QVector>

#include <optional>
#include <stdexcept>

namespace Monitor::Infrastructure::Persistence {

enum class HistorySortDirection {
    Ascending,
    Descending
};

enum class AlarmSortDirection {
    Ascending,
    Descending
};

struct HistoryQuery
{
    QString tagId;
    QDateTime startTimeUtc;
    QDateTime endTimeUtc;
    int page = 1;
    int pageSize = 200;
    HistorySortDirection sortDirection = HistorySortDirection::Ascending;

    static constexpr int MaximumPageSize = 1000;

    void validate() const;
};

struct HistoryQueryResult
{
    QVector<Monitor::Domain::Tags::TagValue> items;
    qint64 totalCount = 0;
    int page = 1;
    int pageSize = 200;

    bool hasPreviousPage() const { return page > 1; }
    bool hasNextPage() const { return static_cast<qint64>(page) * pageSize < totalCount; }
};

struct AlarmQuery
{
    QDateTime startTimeUtc;
    QDateTime endTimeUtc;
    std::optional<QString> tagId;
    std::optional<Monitor::Domain::Alarms::AlarmLevel> level;
    std::optional<Monitor::Domain::Alarms::AlarmState> state;
    int page = 1;
    int pageSize = 200;
    AlarmSortDirection sortDirection = AlarmSortDirection::Descending;

    static constexpr int MaximumPageSize = 1000;

    void validate() const;
};

struct AlarmQueryResult
{
    QVector<Monitor::Domain::Alarms::AlarmEvent> items;
    qint64 totalCount = 0;
    int page = 1;
    int pageSize = 200;

    bool hasPreviousPage() const { return page > 1; }
    bool hasNextPage() const { return static_cast<qint64>(page) * pageSize < totalCount; }
};

struct HistoryRetentionResult
{
    qint64 deletedCount = 0;
    QDateTime cutoffUtc;
};

} // namespace Monitor::Infrastructure::Persistence

#endif // PERSISTENCEMODELS_H
