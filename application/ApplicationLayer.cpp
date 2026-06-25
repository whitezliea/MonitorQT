#include "ApplicationLayer.h"

namespace Monitor::Application {

ApplicationLayerInfo applicationLayerInfo()
{
    return {
        QStringLiteral("MonitorApplication"),
        {
            QStringLiteral("configuration"),
            QStringLiteral("dto"),
            QStringLiteral("events"),
            QStringLiteral("mapping"),
            QStringLiteral("pipelines"),
            QStringLiteral("queues"),
            QStringLiteral("services"),
            QStringLiteral("usecases"),
            QStringLiteral("workers")
        },
        {
            QStringLiteral("MonitorDomain"),
            QStringLiteral("QtCore")
        },
        {
            QStringLiteral("QtWidgets"),
            QStringLiteral("QtSql concrete repositories"),
            QStringLiteral("Simulator internals")
        }
    };
}

} // namespace Monitor::Application
