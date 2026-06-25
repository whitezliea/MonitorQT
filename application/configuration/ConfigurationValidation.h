#ifndef CONFIGURATIONVALIDATION_H
#define CONFIGURATIONVALIDATION_H

#include "MonitorRuntimeOptions.h"
#include "TagRuntimeConfiguration.h"

#include "domain/tags/TagModels.h"

namespace Monitor::Application::Configuration {

class ConfigurationValidation
{
public:
    static void validateTag(
        const Monitor::Domain::Tags::TagDefinition &definition,
        const TagRuntimeConfiguration &configuration);
    static void validateRuntimeOptions(const MonitorRuntimeOptions &options);

private:
    static void validateBound(
        const QString &tagId,
        const QString &name,
        const std::optional<double> &value,
        const std::optional<double> &minimum,
        const std::optional<double> &maximum);
};

} // namespace Monitor::Application::Configuration

#endif // CONFIGURATIONVALIDATION_H
