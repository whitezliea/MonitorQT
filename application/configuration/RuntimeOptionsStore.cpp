#include "RuntimeOptionsStore.h"

#include <QMutexLocker>

namespace Monitor::Application::Configuration {

RuntimeOptionsStore::RuntimeOptionsStore(const MonitorRuntimeOptions &options)
    : m_snapshot(options)
{
}

MonitorRuntimeOptions RuntimeOptionsStore::snapshot() const
{
    QMutexLocker locker(&m_mutex);
    return m_snapshot;
}

void RuntimeOptionsStore::replace(const MonitorRuntimeOptions &options)
{
    QMutexLocker locker(&m_mutex);
    m_snapshot = options;
}

} // namespace Monitor::Application::Configuration
