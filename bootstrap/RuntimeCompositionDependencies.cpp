#include "RuntimeCompositionDependencies.h"

#include "application/configuration/ConfigurationValidation.h"
#include "application/services/TagDefinitionCatalog.h"

#include <exception>

namespace Monitor::Bootstrap {

RuntimeCompositionDependencies RuntimeCompositionDependencies::createDefault()
{
    RuntimeCompositionDependencies dependencies;
    dependencies.runtimeOptions = Monitor::Application::Configuration::MonitorRuntimeOptions();
    dependencies.databasePath = {};
    dependencies.databaseDriverName = QStringLiteral("QSQLITE");
    dependencies.defaultDeviceId = Monitor::Application::Services::TagDefinitionCatalog::defaultDeviceId();
    dependencies.useSimulatorDataSource = true;
    dependencies.useSqlitePersistence = true;
    return dependencies;
}

QStringList RuntimeCompositionDependencies::validate() const
{
    QStringList errors;

    try {
        Monitor::Application::Configuration::ConfigurationValidation::validateRuntimeOptions(runtimeOptions);
    } catch (const std::exception &exception) {
        errors.append(QStringLiteral("Runtime options are invalid: %1").arg(QString::fromUtf8(exception.what())));
    }

    if (defaultDeviceId.trimmed().isEmpty()) {
        errors.append(QStringLiteral("Default device id cannot be empty."));
    }

    if (useSqlitePersistence && databaseDriverName.trimmed().isEmpty()) {
        errors.append(QStringLiteral("SQLite persistence requires a database driver name."));
    }

    return errors;
}

} // namespace Monitor::Bootstrap
