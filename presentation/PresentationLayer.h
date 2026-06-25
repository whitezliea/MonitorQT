#ifndef PRESENTATIONLAYER_H
#define PRESENTATIONLAYER_H

#include <QString>
#include <QStringList>

namespace Monitor::Presentation {

struct PresentationLayerInfo
{
    QString name;
    QStringList owns;
    QStringList requiredPages;
    QStringList requiredQtModules;
};

PresentationLayerInfo presentationLayerInfo();

} // namespace Monitor::Presentation

#endif // PRESENTATIONLAYER_H
