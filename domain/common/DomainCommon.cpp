#include "DomainCommon.h"

#include <QTimeZone>

namespace Monitor::Domain::Common {

DomainException::DomainException(const QString &message)
    : std::runtime_error(message.toStdString())
{
}

Result Result::success()
{
    return {true, QString()};
}

Result Result::failure(const QString &errorMessage)
{
    return {false, errorMessage};
}

bool TimeRange::contains(const QDateTime &timestampUtc) const
{
    return timestampUtc >= startTimeUtc && timestampUtc <= endTimeUtc;
}

bool UtcDateTime::isUtc(const QDateTime &value)
{
    return value.isValid()
        && (value.timeSpec() == Qt::UTC || value.offsetFromUtc() == 0);
}

QDateTime UtcDateTime::require(const QDateTime &value, const QString &parameterName)
{
    if (!isUtc(value)) {
        throw DomainException(
            QStringLiteral("%1 must use UTC time. Actual timeSpec: %2.")
                .arg(parameterName)
                .arg(static_cast<int>(value.timeSpec())));
    }

    return value.toUTC();
}

std::optional<QDateTime> UtcDateTime::require(const std::optional<QDateTime> &value, const QString &parameterName)
{
    if (!value.has_value()) {
        return std::nullopt;
    }

    return require(value.value(), parameterName);
}

qint64 UtcDateTime::toCSharpTicks(const QDateTime &value)
{
    const auto utc = require(value, QStringLiteral("value"));
    return CSharpTicksAtUnixEpoch + utc.toMSecsSinceEpoch() * TicksPerMillisecond;
}

QDateTime UtcDateTime::fromCSharpTicks(qint64 ticks)
{
    const auto milliseconds = (ticks - CSharpTicksAtUnixEpoch) / TicksPerMillisecond;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QDateTime::fromMSecsSinceEpoch(milliseconds, QTimeZone(QTimeZone::UTC));
#else
    return QDateTime::fromMSecsSinceEpoch(milliseconds, Qt::UTC);
#endif
}

} // namespace Monitor::Domain::Common
