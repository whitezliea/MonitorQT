#include "ApplicationEvents.h"

namespace Monitor::Application::Events {

QString eventName(const ApplicationEvent &event)
{
    return std::visit([](const auto &value) {
        return eventName(value);
    }, event);
}

QString eventName(const RawFrameReceivedEvent &)
{
    return QStringLiteral("RawFrameReceivedEvent");
}

QString eventName(const TagRuntimeStatesProducedEvent &)
{
    return QStringLiteral("TagRuntimeStatesProducedEvent");
}

QString eventName(const AlarmRaisedEvent &)
{
    return QStringLiteral("AlarmRaisedEvent");
}

QString eventName(const AlarmUpdatedEvent &)
{
    return QStringLiteral("AlarmUpdatedEvent");
}

QString eventName(const AlarmRecoveredEvent &)
{
    return QStringLiteral("AlarmRecoveredEvent");
}

QString eventName(const AlarmAcknowledgedEvent &)
{
    return QStringLiteral("AlarmAcknowledgedEvent");
}

QString eventName(const DataSourceTimedOutEvent &)
{
    return QStringLiteral("DataSourceTimedOutEvent");
}

QString eventName(const DataSourceRecoveredEvent &)
{
    return QStringLiteral("DataSourceRecoveredEvent");
}

} // namespace Monitor::Application::Events
