#ifndef RUNTIMEOPTIONSSTORE_H
#define RUNTIMEOPTIONSSTORE_H

#include "MonitorRuntimeOptions.h"

#include <QMutex>

namespace Monitor::Application::Configuration {

class RuntimeOptionsStore
{
public:
    explicit RuntimeOptionsStore(const MonitorRuntimeOptions &options = MonitorRuntimeOptions());

    MonitorRuntimeOptions snapshot() const;
    void replace(const MonitorRuntimeOptions &options);

private:
    mutable QMutex m_mutex;
    MonitorRuntimeOptions m_snapshot;
};

} // namespace Monitor::Application::Configuration

#endif // RUNTIMEOPTIONSSTORE_H
