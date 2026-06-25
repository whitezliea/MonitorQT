#ifndef APPLICATIONEVENTS_H
#define APPLICATIONEVENTS_H

#include "domain/alarms/AlarmModels.h"
#include "domain/measurements/MeasurementModels.h"
#include "domain/tags/TagModels.h"

#include <QDateTime>
#include <QUuid>
#include <QVector>

#include <variant>

namespace Monitor::Application::Events {

struct RawFrameReceivedEvent
{
    Monitor::Domain::Measurements::RawMeasurementFrame frame;
};

struct TagRuntimeStatesProducedEvent
{
    QUuid sourceFrameId;
    qint64 sequenceNo = 0;
    QDateTime timestampUtc;
    QVector<Monitor::Domain::Tags::TagRuntimeState> states;
};

struct AlarmRaisedEvent
{
    Monitor::Domain::Alarms::AlarmEvent alarm;
};

struct AlarmUpdatedEvent
{
    Monitor::Domain::Alarms::AlarmEvent alarm;
};

struct AlarmRecoveredEvent
{
    Monitor::Domain::Alarms::AlarmEvent alarm;
};

struct AlarmAcknowledgedEvent
{
    Monitor::Domain::Alarms::AlarmEvent alarm;
};

struct DataSourceTimedOutEvent
{
    QUuid lastFrameId;
    qint64 lastSequenceNo = 0;
    QDateTime lastFrameTimeUtc;
    QDateTime timedOutAtUtc;
};

struct DataSourceRecoveredEvent
{
    QUuid frameId;
    qint64 sequenceNo = 0;
    QDateTime recoveredAtUtc;
};

using ApplicationEvent = std::variant<
    RawFrameReceivedEvent,
    TagRuntimeStatesProducedEvent,
    AlarmRaisedEvent,
    AlarmUpdatedEvent,
    AlarmRecoveredEvent,
    AlarmAcknowledgedEvent,
    DataSourceTimedOutEvent,
    DataSourceRecoveredEvent>;

QString eventName(const ApplicationEvent &event);
QString eventName(const RawFrameReceivedEvent &event);
QString eventName(const TagRuntimeStatesProducedEvent &event);
QString eventName(const AlarmRaisedEvent &event);
QString eventName(const AlarmUpdatedEvent &event);
QString eventName(const AlarmRecoveredEvent &event);
QString eventName(const AlarmAcknowledgedEvent &event);
QString eventName(const DataSourceTimedOutEvent &event);
QString eventName(const DataSourceRecoveredEvent &event);

} // namespace Monitor::Application::Events

#endif // APPLICATIONEVENTS_H
