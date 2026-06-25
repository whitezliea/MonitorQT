#ifndef DOMAINLAYER_H
#define DOMAINLAYER_H

#include <QString>
#include <QStringList>

namespace Monitor::Domain {

struct DomainLayerInfo
{
    QString name;
    QStringList owns;
    QStringList forbiddenDependencies;
};

DomainLayerInfo domainLayerInfo();

} // namespace Monitor::Domain

#endif // DOMAINLAYER_H
