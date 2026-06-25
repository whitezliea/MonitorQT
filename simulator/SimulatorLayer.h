#ifndef SIMULATORLAYER_H
#define SIMULATORLAYER_H

#include <QString>
#include <QStringList>

namespace Monitor::Simulator {

struct SimulatorLayerInfo
{
    QString name;
    QStringList owns;
    QStringList allowedUpstreamTargets;
    QStringList forbiddenDependencies;
};

SimulatorLayerInfo simulatorLayerInfo();

} // namespace Monitor::Simulator

#endif // SIMULATORLAYER_H
