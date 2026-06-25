#include "PersistenceModels.h"

namespace Monitor::Infrastructure::Persistence {
namespace {

void throwArgument(const QString &message)
{
    throw std::invalid_argument(message.toStdString());
}

void validateRange(const QDateTime &startTimeUtc, const QDateTime &endTimeUtc, const QString &label)
{
    Monitor::Domain::Common::UtcDateTime::require(startTimeUtc, label + QStringLiteral(".startTimeUtc"));
    Monitor::Domain::Common::UtcDateTime::require(endTimeUtc, label + QStringLiteral(".endTimeUtc"));
    if (startTimeUtc > endTimeUtc) {
        throwArgument(label + QStringLiteral(" start time must not be later than end time."));
    }

    if (startTimeUtc.daysTo(endTimeUtc) > 366) {
        throwArgument(label + QStringLiteral(" range must not exceed 366 days."));
    }
}

void validatePage(int page, int pageSize, int maximumPageSize)
{
    if (page <= 0) {
        throw std::out_of_range("Page must be greater than zero.");
    }

    if (pageSize <= 0 || pageSize > maximumPageSize) {
        throw std::out_of_range("PageSize is out of range.");
    }
}

} // namespace

void HistoryQuery::validate() const
{
    if (tagId.trimmed().isEmpty()) {
        throwArgument(QStringLiteral("History query tag id cannot be empty."));
    }

    validateRange(startTimeUtc, endTimeUtc, QStringLiteral("History query"));
    validatePage(page, pageSize, MaximumPageSize);
}

void AlarmQuery::validate() const
{
    validateRange(startTimeUtc, endTimeUtc, QStringLiteral("Alarm query"));
    validatePage(page, pageSize, MaximumPageSize);
}

} // namespace Monitor::Infrastructure::Persistence
