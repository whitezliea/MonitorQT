#include "SimulatorLayer.h"

namespace Monitor::Simulator {

SimulatorLayerInfo simulatorLayerInfo()
{
    return {
        QStringLiteral("MonitorSimulator"),
        {
            QStringLiteral("adapters"),
            QStringLiteral("generators"),
            QStringLiteral("models"),
            QStringLiteral("noise"),
            QStringLiteral("profiles"),
            QStringLiteral("scenarios")
        },
        {
            QStringLiteral("MonitorApplication"),
            QStringLiteral("MonitorDomain"),
            QStringLiteral("QtCore")
        },
        {
            QStringLiteral("QtWidgets"),
            QStringLiteral("QtSql"),
            QStringLiteral("Presentation")
        }
    };
}

} // namespace Monitor::Simulator
