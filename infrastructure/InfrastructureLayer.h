#ifndef INFRASTRUCTURELAYER_H
#define INFRASTRUCTURELAYER_H

#include <QString>
#include <QStringList>

namespace Monitor::Infrastructure {

struct InfrastructureLayerInfo
{
    QString name;
    QStringList owns;
    QStringList allowedUpstreamTargets;
    QStringList requiredQtModules;
};

InfrastructureLayerInfo infrastructureLayerInfo();

} // namespace Monitor::Infrastructure

#endif // INFRASTRUCTURELAYER_H
