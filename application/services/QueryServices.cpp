#include "QueryServices.h"

#include "domain/common/DomainCommon.h"

#include <stdexcept>
#include <utility>

namespace Monitor::Application::Services {
namespace {

void validateRange(const QDateTime &startTimeUtc, const QDateTime &endTimeUtc, const QString &name)
{
    Monitor::Domain::Common::UtcDateTime::require(startTimeUtc, name + QStringLiteral(".startTimeUtc"));
    Monitor::Domain::Common::UtcDateTime::require(endTimeUtc, name + QStringLiteral(".endTimeUtc"));
    if (startTimeUtc > endTimeUtc) {
        throw std::invalid_argument(QStringLiteral("%1 start time must not be later than end time.").arg(name).toStdString());
    }
}

void validatePage(int page, int pageSize, const QString &name)
{
    if (page <= 0) {
        throw std::out_of_range(QStringLiteral("%1 page must be greater than zero.").arg(name).toStdString());
    }
    if (pageSize <= 0 || pageSize > 1000) {
        throw std::out_of_range(QStringLiteral("%1 page size must be between 1 and 1000.").arg(name).toStdString());
    }
}

} // namespace

HistoryQueryService::HistoryQueryService(QueryFunction query)
    : m_query(std::move(query))
{
    if (!m_query) {
        throw std::invalid_argument("HistoryQueryService query function must not be empty.");
    }
}

HistoryQueryPage HistoryQueryService::query(const HistoryQueryRequest &request) const
{
    if (request.tagId.trimmed().isEmpty()) {
        throw std::invalid_argument("History query tagId must not be empty.");
    }
    validateRange(request.startTimeUtc, request.endTimeUtc, QStringLiteral("HistoryQuery"));
    validatePage(request.page, request.pageSize, QStringLiteral("HistoryQuery"));
    return m_query(request);
}

AlarmQueryService::AlarmQueryService(QueryFunction query)
    : m_query(std::move(query))
{
    if (!m_query) {
        throw std::invalid_argument("AlarmQueryService query function must not be empty.");
    }
}

AlarmHistoryQueryPage AlarmQueryService::query(const AlarmHistoryQueryRequest &request) const
{
    validateRange(request.startTimeUtc, request.endTimeUtc, QStringLiteral("AlarmHistoryQuery"));
    validatePage(request.page, request.pageSize, QStringLiteral("AlarmHistoryQuery"));
    return m_query(request);
}

OperationLogQueryService::OperationLogQueryService(QueryFunction query)
    : m_query(std::move(query))
{
    if (!m_query) {
        throw std::invalid_argument("OperationLogQueryService query function must not be empty.");
    }
}

OperationLogQueryPage OperationLogQueryService::query(const OperationLogQueryRequest &request) const
{
    validateRange(request.startTimeUtc, request.endTimeUtc, QStringLiteral("OperationLogQuery"));
    validatePage(request.page, request.pageSize, QStringLiteral("OperationLogQuery"));
    return m_query(request);
}

} // namespace Monitor::Application::Services
