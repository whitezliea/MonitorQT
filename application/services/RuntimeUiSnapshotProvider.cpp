#include "RuntimeUiSnapshotProvider.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace Monitor::Application::Services {

RuntimeUiSnapshotProvider::RuntimeUiSnapshotProvider(
    const Monitor::Application::Runtime::RuntimeLifecycleCoordinator *runtimeLifecycle,
    const Monitor::Application::Runtime::PersistenceRuntimeCoordinator *persistenceRuntime,
    const Monitor::Application::Runtime::DataSourceHealthMonitor *dataSourceHealthMonitor,
    const Monitor::Application::Configuration::RuntimeOptionsStore *runtimeOptionsStore,
    const Monitor::Application::Configuration::TagRuntimeConfigurationStore *tagRuntimeConfigurationStore,
    const TagService *tagService,
    const AlarmService *alarmService,
    const DashboardService *dashboardService,
    const MeasurementMapService *measurementMapService,
    QVector<Monitor::Domain::Tags::TagDefinition> tagDefinitions,
    bool databaseAvailable)
    : m_runtimeLifecycle(runtimeLifecycle),
      m_persistenceRuntime(persistenceRuntime),
      m_dataSourceHealthMonitor(dataSourceHealthMonitor),
      m_runtimeOptionsStore(runtimeOptionsStore),
      m_tagRuntimeConfigurationStore(tagRuntimeConfigurationStore),
      m_tagService(tagService),
      m_alarmService(alarmService),
      m_dashboardService(dashboardService),
      m_measurementMapService(measurementMapService),
      m_tagDefinitions(std::move(tagDefinitions)),
      m_databaseAvailable(databaseAvailable)
{
    if (!m_runtimeLifecycle ||
        !m_persistenceRuntime ||
        !m_dataSourceHealthMonitor ||
        !m_runtimeOptionsStore ||
        !m_tagRuntimeConfigurationStore ||
        !m_tagService ||
        !m_alarmService ||
        !m_dashboardService ||
        !m_measurementMapService) {
        throw std::invalid_argument("RuntimeUiSnapshotProvider dependencies must not be null.");
    }
}

Monitor::Application::Dtos::UiSnapshot RuntimeUiSnapshotProvider::refresh() const
{
    const auto capturedAtUtc = QDateTime::currentDateTimeUtc();
    const auto runtimeStatus = m_runtimeLifecycle->status();
    const auto persistenceStatus = m_persistenceRuntime->status();
    const auto healthStatus = m_dataSourceHealthMonitor->status();
    const auto tagSnapshot = m_tagService->snapshot();
    const auto currentAlarms = m_alarmService->currentAlarms();
    const auto measurementMap = m_measurementMapService->latestSnapshot();

    Monitor::Application::Dtos::UiSnapshot snapshot;
    snapshot.shell.running = runtimeStatus.state == Monitor::Application::Runtime::RuntimeLifecycleState::Starting ||
        runtimeStatus.state == Monitor::Application::Runtime::RuntimeLifecycleState::Running;
    snapshot.shell.dataSourceConnected = healthStatus.state == Monitor::Application::Runtime::DataSourceHealthState::Online ||
        healthStatus.state == Monitor::Application::Runtime::DataSourceHealthState::WaitingForFirstFrame;
    snapshot.shell.databaseConnected = m_databaseAvailable &&
        persistenceStatus.state != Monitor::Application::Runtime::PersistenceRuntimeState::Faulted;
    snapshot.shell.lastFrameIndex = healthStatus.lastSequenceNo > 0
        ? static_cast<quint64>(healthStatus.lastSequenceNo)
        : latestSequenceNo(tagSnapshot);
    snapshot.shell.matrixFrameIndex = measurementMap.has_value()
        ? measurementMap->sequenceNo
        : healthStatus.lastSequenceNo;
    snapshot.shell.syncState = syncState(runtimeStatus, persistenceStatus, healthStatus);
    snapshot.shell.capturedAtUtc = capturedAtUtc;
    snapshot.tags = tagSnapshot;
    snapshot.dashboard = m_dashboardService->buildSnapshot(
        tagSnapshot,
        currentAlarms,
        capturedAtUtc);
    snapshot.currentAlarms = currentAlarms;
    snapshot.alarmHistory = m_alarmService->recentAlarmEvents();
    snapshot.measurementMap = measurementMap;
    snapshot.runtimeOptions = m_runtimeOptionsStore->snapshot();
    snapshot.tagConfigurations = tagConfigurations();
    snapshot.tagDefinitions = m_tagDefinitions;
    return snapshot;
}

QString RuntimeUiSnapshotProvider::syncState(
    const Monitor::Application::Runtime::RuntimeLifecycleStatus &runtimeStatus,
    const Monitor::Application::Runtime::PersistenceRuntimeStatus &persistenceStatus,
    const Monitor::Application::Runtime::DataSourceHealthStatus &healthStatus) const
{
    if (runtimeStatus.state == Monitor::Application::Runtime::RuntimeLifecycleState::Faulted) {
        return QStringLiteral("Faulted: %1").arg(runtimeStatus.errorMessage);
    }

    if (persistenceStatus.state == Monitor::Application::Runtime::PersistenceRuntimeState::Faulted) {
        return persistenceStatus.workerName.isEmpty()
            ? QStringLiteral("PersistenceFaulted")
            : QStringLiteral("PersistenceFaulted: %1").arg(persistenceStatus.workerName);
    }

    if (healthStatus.state == Monitor::Application::Runtime::DataSourceHealthState::TimedOut) {
        return QStringLiteral("TimedOut");
    }

    if (runtimeStatus.state == Monitor::Application::Runtime::RuntimeLifecycleState::Running) {
        return healthStatus.state == Monitor::Application::Runtime::DataSourceHealthState::Online
            ? QStringLiteral("Streaming")
            : Monitor::Application::Runtime::toString(healthStatus.state);
    }

    if (runtimeStatus.state == Monitor::Application::Runtime::RuntimeLifecycleState::Starting ||
        runtimeStatus.state == Monitor::Application::Runtime::RuntimeLifecycleState::Stopping) {
        return Monitor::Application::Runtime::toString(runtimeStatus.state);
    }

    if (persistenceStatus.state == Monitor::Application::Runtime::PersistenceRuntimeState::Running ||
        persistenceStatus.state == Monitor::Application::Runtime::PersistenceRuntimeState::Degraded) {
        return QStringLiteral("Idle");
    }

    return QStringLiteral("Stopped");
}

QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> RuntimeUiSnapshotProvider::tagConfigurations() const
{
    const auto snapshot = m_tagRuntimeConfigurationStore->snapshot();
    QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> configurations;
    configurations.reserve(m_tagDefinitions.size());
    for (const auto &definition : m_tagDefinitions) {
        const auto it = snapshot.constFind(definition.tagId);
        if (it != snapshot.cend()) {
            configurations.append(it.value());
        }
    }

    return configurations;
}

quint64 RuntimeUiSnapshotProvider::latestSequenceNo(
    const Monitor::Domain::Tags::TagSnapshot &tagSnapshot) const
{
    qint64 latest = 0;
    for (const auto &state : tagSnapshot.currentValues) {
        latest = std::max(latest, state.sequenceNo);
    }

    return latest > 0 ? static_cast<quint64>(latest) : 0;
}

} // namespace Monitor::Application::Services
