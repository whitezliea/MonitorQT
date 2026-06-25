#include "TaskModels.h"

namespace Monitor::Domain::Tasks {

QString toString(MeasurementTaskStatus status)
{
    switch (status) {
    case MeasurementTaskStatus::Created:
        return QStringLiteral("Created");
    case MeasurementTaskStatus::Running:
        return QStringLiteral("Running");
    case MeasurementTaskStatus::Completed:
        return QStringLiteral("Completed");
    case MeasurementTaskStatus::Failed:
        return QStringLiteral("Failed");
    case MeasurementTaskStatus::Canceled:
        return QStringLiteral("Canceled");
    }

    return QString();
}

} // namespace Monitor::Domain::Tasks
