#include "InfrastructureLayer.h"

namespace Monitor::Infrastructure {

InfrastructureLayerInfo infrastructureLayerInfo()
{
    return {
        QStringLiteral("MonitorInfrastructure"),
        {
            QStringLiteral("persistence"),
            QStringLiteral("logging"),
            QStringLiteral("export"),
            QStringLiteral("datasource"),
            QStringLiteral("system")
        },
        {
            QStringLiteral("MonitorApplication"),
            QStringLiteral("MonitorDomain")
        },
        {
            QStringLiteral("QtCore"),
            QStringLiteral("QtSql")
        }
    };
}

} // namespace Monitor::Infrastructure
