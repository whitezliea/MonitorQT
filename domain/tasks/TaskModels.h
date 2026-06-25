#ifndef TASKMODELS_H
#define TASKMODELS_H

#include <QDateTime>
#include <QString>
#include <QUuid>

namespace Monitor::Domain::Tasks {

enum class MeasurementTaskStatus {
    Created,
    Running,
    Completed,
    Failed,
    Canceled
};

struct MeasurementTask
{
    QUuid id;
    QString taskName;
    QDateTime startTimeUtc;
    MeasurementTaskStatus status = MeasurementTaskStatus::Created;
};

struct MeasurementTaskSummary
{
    int totalCount = 0;
    int runningCount = 0;
    int completedCount = 0;
    int failedCount = 0;
};

QString toString(MeasurementTaskStatus status);

} // namespace Monitor::Domain::Tasks

#endif // TASKMODELS_H
