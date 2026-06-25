#include "RuntimeCompositionDependencies.h"

namespace Monitor::Bootstrap {

RuntimeCompositionDependencies RuntimeCompositionDependencies::createDefault()
{
    RuntimeCompositionDependencies dependencies;
    dependencies.runtimeOptions = Phase0::defaultRuntimeOptions();
    dependencies.databaseDriverName = QStringLiteral("QSQLITE");
    dependencies.defaultDeviceId = Phase0::defaultDeviceId();
    dependencies.useSimulatorDataSource = true;
    dependencies.useSqlitePersistence = true;
    return dependencies;
}

QStringList RuntimeCompositionDependencies::validate() const
{
    QStringList errors;

    if (runtimeOptions.dataGenerateIntervalMs <= 0) {
        errors.append(QStringLiteral("DataGenerateIntervalMs must be greater than zero."));
    }

    if (runtimeOptions.uiRefreshIntervalMs <= 0) {
        errors.append(QStringLiteral("UiRefreshIntervalMs must be greater than zero."));
    }

    if (runtimeOptions.historyBatchIntervalMs <= 0 ||
        runtimeOptions.alarmBatchIntervalMs <= 0 ||
        runtimeOptions.operationLogBatchIntervalMs <= 0) {
        errors.append(QStringLiteral("Batch intervals must be greater than zero."));
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
