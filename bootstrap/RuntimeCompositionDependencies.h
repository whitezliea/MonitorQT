#ifndef RUNTIMECOMPOSITIONDEPENDENCIES_H
#define RUNTIMECOMPOSITIONDEPENDENCIES_H

#include "application/configuration/MonitorRuntimeOptions.h"

#include <QString>
#include <QStringList>

namespace Monitor::Bootstrap {

struct RuntimeCompositionDependencies
{
    Monitor::Application::Configuration::MonitorRuntimeOptions runtimeOptions;
    QString databaseDriverName = QStringLiteral("QSQLITE");
    QString defaultDeviceId;
    bool useSimulatorDataSource = true;
    bool useSqlitePersistence = true;

    static RuntimeCompositionDependencies createDefault();
    QStringList validate() const;
};

} // namespace Monitor::Bootstrap

#endif // RUNTIMECOMPOSITIONDEPENDENCIES_H
