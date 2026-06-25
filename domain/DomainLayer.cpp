#include "DomainLayer.h"

namespace Monitor::Domain {

DomainLayerInfo domainLayerInfo()
{
    return {
        QStringLiteral("MonitorDomain"),
        {
            QStringLiteral("devices"),
            QStringLiteral("measurements"),
            QStringLiteral("tags"),
            QStringLiteral("alarms"),
            QStringLiteral("rules"),
            QStringLiteral("logs"),
            QStringLiteral("tasks")
        },
        {
            QStringLiteral("QObject"),
            QStringLiteral("QWidget"),
            QStringLiteral("SQLite"),
            QStringLiteral("UI"),
            QStringLiteral("Simulator")
        }
    };
}

} // namespace Monitor::Domain
