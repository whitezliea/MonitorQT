#ifndef MONITORINGRUNTIMESERVICE_H
#define MONITORINGRUNTIMESERVICE_H

#include "application/abstractions/IRawFrameSource.h"
#include "application/configuration/MonitorRuntimeOptions.h"
#include "application/events/EventBus.h"
#include "application/pipelines/DataCleanPipeline.h"
#include "application/runtime/DataSourceHealthMonitor.h"
#include "application/services/AlarmService.h"

#include <QDateTime>

#include <atomic>

namespace Monitor::Application::Runtime {

class MonitoringRuntimeService
{
public:
    MonitoringRuntimeService(
        Monitor::Application::Abstractions::IRawFrameSource *dataSource,
        Monitor::Application::Pipelines::DataCleanPipeline *dataCleanPipeline,
        Monitor::Application::Services::AlarmService *alarmService,
        Monitor::Application::EventBus *eventBus,
        const Monitor::Application::Configuration::MonitorRuntimeOptions &runtimeOptions,
        DataSourceHealthMonitor *healthMonitor = nullptr);

    void run(std::atomic_bool &stopRequested);
    bool processFrame(const Monitor::Domain::Measurements::RawMeasurementFrame &frame, QStringList *errors = nullptr);
    bool publishOfflineStatesIfTimedOut(const QDateTime &nowUtc, QStringList *errors = nullptr);
    bool acknowledgeAlarm(
        const QUuid &alarmId,
        const QDateTime &acknowledgedAtUtc,
        Monitor::Domain::Alarms::AlarmEvent *acknowledgedAlarm = nullptr,
        QStringList *errors = nullptr);
    DataSourceHealthMonitor &healthMonitor();

private:
    bool publishEvaluatedStates(
        const QUuid &frameId,
        qint64 sequenceNo,
        const QDateTime &timestampUtc,
        const QVector<Monitor::Domain::Tags::CleanedTagValue> &cleanedValues,
        QStringList *errors);
    bool publishAlarmChange(
        const Monitor::Application::Dtos::AlarmLifecycleChange &change,
        QStringList *errors);
    QVector<Monitor::Domain::Tags::CleanedTagValue> createOfflineValues(
        const Monitor::Application::Events::DataSourceTimedOutEvent &timedOutEvent) const;

    Monitor::Application::Abstractions::IRawFrameSource *m_dataSource = nullptr;
    Monitor::Application::Pipelines::DataCleanPipeline *m_dataCleanPipeline = nullptr;
    Monitor::Application::Services::AlarmService *m_alarmService = nullptr;
    Monitor::Application::EventBus *m_eventBus = nullptr;
    Monitor::Application::Configuration::MonitorRuntimeOptions m_runtimeOptions;
    DataSourceHealthMonitor m_ownedHealthMonitor;
    DataSourceHealthMonitor *m_healthMonitor = nullptr;
    qint64 m_sessionId = 0;
    QVector<Monitor::Domain::Tags::CleanedTagValue> m_lastCleanedValues;
};

} // namespace Monitor::Application::Runtime

#endif // MONITORINGRUNTIMESERVICE_H
