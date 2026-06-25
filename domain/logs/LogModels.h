#ifndef LOGMODELS_H
#define LOGMODELS_H

#include <QDateTime>
#include <QString>
#include <QVector>

#include <optional>

namespace Monitor::Domain::Logs {

enum class LogSource {
    Runtime,
    Ui,
    Persistence,
    Simulator
};

enum class OperationLogLevel {
    Debug,
    Info,
    Warning,
    Error
};

enum class OperationLogCategory {
    System,
    User,
    Alarm,
    Database,
    Export,
    Settings
};

struct OperationLog
{
    QDateTime timestampUtc;
    OperationLogLevel level = OperationLogLevel::Info;
    QString category;
    QString message;
    QString action;
    QString source;
    std::optional<QString> detail;
    std::optional<QString> correlationId;
    qint64 id = 0;
};

struct OperationLogQuery
{
    QDateTime startTimeUtc;
    QDateTime endTimeUtc;
    std::optional<OperationLogLevel> level;
    std::optional<QString> category;
    int maxCount = 200;
    int page = 1;
    int pageSize = 200;
};

struct OperationLogQueryResult
{
    QVector<OperationLog> items;
    qint64 totalCount = 0;
    int page = 1;
    int pageSize = 200;

    bool hasPreviousPage() const { return page > 1; }
    bool hasNextPage() const { return static_cast<qint64>(page) * pageSize < totalCount; }
};

QString toString(OperationLogLevel level);
QString toString(OperationLogCategory category);

} // namespace Monitor::Domain::Logs

#endif // LOGMODELS_H
