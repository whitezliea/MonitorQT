#ifndef TAGDEFINITIONCATALOG_H
#define TAGDEFINITIONCATALOG_H

#include "domain/tags/TagModels.h"

#include <QString>
#include <QVector>

namespace Monitor::Application::Services {

class TagDefinitionCatalog
{
public:
    static const QString &defaultDeviceId();
    static QVector<Monitor::Domain::Tags::TagDefinition> createDefaults();
    static QVector<Monitor::Domain::Tags::TagSourceMapping> createSourceMappings(
        const QString &sourceDeviceId = defaultDeviceId());
};

} // namespace Monitor::Application::Services

#endif // TAGDEFINITIONCATALOG_H
