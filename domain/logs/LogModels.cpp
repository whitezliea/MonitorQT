#include "LogModels.h"

namespace Monitor::Domain::Logs {

QString toString(OperationLogLevel level)
{
    switch (level) {
    case OperationLogLevel::Debug:
        return QStringLiteral("Debug");
    case OperationLogLevel::Info:
        return QStringLiteral("Info");
    case OperationLogLevel::Warning:
        return QStringLiteral("Warning");
    case OperationLogLevel::Error:
        return QStringLiteral("Error");
    }

    return QString();
}

QString toString(OperationLogCategory category)
{
    switch (category) {
    case OperationLogCategory::System:
        return QStringLiteral("System");
    case OperationLogCategory::User:
        return QStringLiteral("User");
    case OperationLogCategory::Alarm:
        return QStringLiteral("Alarm");
    case OperationLogCategory::Database:
        return QStringLiteral("Database");
    case OperationLogCategory::Export:
        return QStringLiteral("Export");
    case OperationLogCategory::Settings:
        return QStringLiteral("Settings");
    }

    return QString();
}

} // namespace Monitor::Domain::Logs
