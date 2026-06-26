#ifndef PERSISTENCERUNTIMECOORDINATOR_H
#define PERSISTENCERUNTIMECOORDINATOR_H

#include "application/workers/BatchPersistWorker.h"

#include <QVector>

namespace Monitor::Application::Runtime {

enum class PersistenceRuntimeState {
    Stopped,
    Running,
    Degraded,
    Faulted
};

struct PersistenceRuntimeStatus
{
    PersistenceRuntimeState state = PersistenceRuntimeState::Stopped;
    QString workerName;
    QString errorMessage;
};

class PersistenceRuntimeCoordinator
{
public:
    explicit PersistenceRuntimeCoordinator(const QVector<Monitor::Application::Workers::IPersistWorker *> &workers);
    ~PersistenceRuntimeCoordinator();

    bool start();
    bool stop();
    bool flushWorker(const QString &workerName);
    bool flushHistory();
    bool flushAlarms();
    bool flushOperationLogs();
    bool isRunning() const;
    PersistenceRuntimeStatus status() const;

private:
    void refreshStatus();
    void setStatus(const PersistenceRuntimeStatus &status);

    QVector<Monitor::Application::Workers::IPersistWorker *> m_workers;
    PersistenceRuntimeStatus m_status;
};

QString toString(PersistenceRuntimeState state);

} // namespace Monitor::Application::Runtime

#endif // PERSISTENCERUNTIMECOORDINATOR_H
