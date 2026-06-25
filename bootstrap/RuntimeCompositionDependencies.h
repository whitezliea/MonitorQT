#ifndef RUNTIMECOMPOSITIONDEPENDENCIES_H
#define RUNTIMECOMPOSITIONDEPENDENCIES_H

#include "phase0/SourceBehaviorFreeze.h"

#include <QString>
#include <QStringList>

namespace Monitor::Bootstrap {

struct RuntimeCompositionDependencies
{
    Phase0::RuntimeOptions runtimeOptions;
    QString databaseDriverName = QStringLiteral("QSQLITE");
    QString defaultDeviceId = Phase0::defaultDeviceId();
    bool useSimulatorDataSource = true;
    bool useSqlitePersistence = true;

    static RuntimeCompositionDependencies createDefault();
    QStringList validate() const;
};

} // namespace Monitor::Bootstrap

#endif // RUNTIMECOMPOSITIONDEPENDENCIES_H
