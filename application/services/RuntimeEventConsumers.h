#ifndef RUNTIMEEVENTCONSUMERS_H
#define RUNTIMEEVENTCONSUMERS_H

#include "application/configuration/TagRuntimeConfiguration.h"
#include "application/events/EventBus.h"
#include "application/queues/ApplicationQueues.h"
#include "application/services/MeasurementMapService.h"
#include "application/services/OperationLogService.h"
#include "application/services/TagService.h"
#include "domain/tags/TagModels.h"

#include <QHash>

namespace Monitor::Application::Services {

class TagCacheConsumer
{
public:
    explicit TagCacheConsumer(TagService *tagService);
    void handle(const Monitor::Application::Events::ApplicationEvent &event) const;

private:
    TagService *m_tagService = nullptr;
};

class MeasurementMapFrameConsumer
{
public:
    explicit MeasurementMapFrameConsumer(MeasurementMapService *measurementMapService);
    void handle(const Monitor::Application::Events::ApplicationEvent &event) const;

private:
    MeasurementMapService *m_measurementMapService = nullptr;
};

class HistoryRuntimeStateConsumer
{
public:
    HistoryRuntimeStateConsumer(
        Monitor::Application::Queues::HistorySampleQueue *queue,
        const QVector<Monitor::Domain::Tags::TagDefinition> &definitions);
    HistoryRuntimeStateConsumer(
        Monitor::Application::Queues::HistorySampleQueue *queue,
        const QVector<Monitor::Domain::Tags::TagDefinition> &definitions,
        const QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> &configurations);

    void handle(const Monitor::Application::Events::ApplicationEvent &event);

private:
    struct SamplingState
    {
        QDateTime timestampUtc;
        Monitor::Domain::Tags::TagQuality quality = Monitor::Domain::Tags::TagQuality::Good;
        Monitor::Domain::Tags::TagAlarmState alarmState = Monitor::Domain::Tags::TagAlarmState::Normal;
        qint64 revision = 0;
    };

    bool shouldPersist(
        const Monitor::Domain::Tags::TagRuntimeState &state,
        const Monitor::Application::Configuration::TagRuntimeConfiguration &configuration);

    Monitor::Application::Queues::HistorySampleQueue *m_queue = nullptr;
    QHash<QString, Monitor::Application::Configuration::TagRuntimeConfiguration> m_configurations;
    QHash<QString, SamplingState> m_lastPersisted;
};

class AlarmEventConsumer
{
public:
    explicit AlarmEventConsumer(Monitor::Application::Queues::AlarmEventQueue *queue);
    void handle(const Monitor::Application::Events::ApplicationEvent &event) const;

private:
    Monitor::Application::Queues::AlarmEventQueue *m_queue = nullptr;
};

class AlarmOperationLogConsumer
{
public:
    explicit AlarmOperationLogConsumer(OperationLogService *operationLogService);
    void handle(const Monitor::Application::Events::ApplicationEvent &event) const;

private:
    void write(
        const Monitor::Domain::Alarms::AlarmEvent &alarm,
        const QString &action,
        const QString &message) const;

    OperationLogService *m_operationLogService = nullptr;
};

class DataSourceHealthOperationLogConsumer
{
public:
    explicit DataSourceHealthOperationLogConsumer(OperationLogService *operationLogService);
    void handle(const Monitor::Application::Events::ApplicationEvent &event) const;

private:
    OperationLogService *m_operationLogService = nullptr;
};

bool registerDefaultRuntimeConsumers(
    Monitor::Application::EventBus *eventBus,
    TagCacheConsumer *tagCacheConsumer,
    MeasurementMapFrameConsumer *measurementMapFrameConsumer,
    HistoryRuntimeStateConsumer *historyRuntimeStateConsumer,
    AlarmEventConsumer *alarmEventConsumer,
    AlarmOperationLogConsumer *alarmOperationLogConsumer,
    DataSourceHealthOperationLogConsumer *dataSourceHealthOperationLogConsumer,
    QStringList *errors = nullptr);

} // namespace Monitor::Application::Services

#endif // RUNTIMEEVENTCONSUMERS_H
