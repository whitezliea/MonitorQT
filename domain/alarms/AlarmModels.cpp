#include "AlarmModels.h"

namespace Monitor::Domain::Alarms {

AlarmDefinition AlarmDefinition::fromTagDefinition(const Tags::TagDefinition &definition)
{
    return {
        definition.tagId,
        definition.warningHigh,
        definition.alarmHigh,
        definition.warningLow,
        definition.alarmLow,
        0.0,
        1
    };
}

QString toString(AlarmLevel level)
{
    switch (level) {
    case AlarmLevel::Warning:
        return QStringLiteral("Warning");
    case AlarmLevel::Alarm:
        return QStringLiteral("Alarm");
    case AlarmLevel::Critical:
        return QStringLiteral("Critical");
    case AlarmLevel::Quality:
        return QStringLiteral("Quality");
    }

    return QString();
}

QString toString(AlarmState state)
{
    switch (state) {
    case AlarmState::Active:
        return QStringLiteral("Active");
    case AlarmState::Acknowledged:
        return QStringLiteral("Acknowledged");
    case AlarmState::Recovered:
        return QStringLiteral("Recovered");
    }

    return QString();
}

} // namespace Monitor::Domain::Alarms
