#ifndef APPLICATIONLAYER_H
#define APPLICATIONLAYER_H

#include <QString>
#include <QStringList>

namespace Monitor::Application {

struct ApplicationLayerInfo
{
    QString name;
    QStringList owns;
    QStringList allowedUpstreamTargets;
    QStringList forbiddenDependencies;
};

ApplicationLayerInfo applicationLayerInfo();
QStringList validateApplicationLayer();

} // namespace Monitor::Application

#endif // APPLICATIONLAYER_H
