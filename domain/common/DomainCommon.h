#ifndef DOMAINCOMMON_H
#define DOMAINCOMMON_H

#include <QDateTime>
#include <QString>
#include <QUuid>

#include <optional>
#include <stdexcept>

namespace Monitor::Domain::Common {

class DomainException : public std::runtime_error
{
public:
    explicit DomainException(const QString &message);
};

struct Entity
{
    QUuid id;
};

struct Result
{
    bool isSuccess = true;
    QString errorMessage;

    static Result success();
    static Result failure(const QString &errorMessage);
};

struct TimeRange
{
    QDateTime startTimeUtc;
    QDateTime endTimeUtc;

    bool contains(const QDateTime &timestampUtc) const;
};

class UtcDateTime
{
public:
    static constexpr qint64 CSharpTicksAtUnixEpoch = 621355968000000000LL;
    static constexpr qint64 TicksPerMillisecond = 10000LL;

    static bool isUtc(const QDateTime &value);
    static QDateTime require(const QDateTime &value, const QString &parameterName);
    static std::optional<QDateTime> require(const std::optional<QDateTime> &value, const QString &parameterName);
    static qint64 toCSharpTicks(const QDateTime &value);
    static QDateTime fromCSharpTicks(qint64 ticks);
};

} // namespace Monitor::Domain::Common

#endif // DOMAINCOMMON_H
