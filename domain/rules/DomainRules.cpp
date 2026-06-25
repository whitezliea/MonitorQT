#include "DomainRules.h"

namespace Monitor::Domain::Rules {

Tags::TagQuality QualityRule::fromDeviceState(Devices::DeviceStatus status, int errorCode)
{
    if (status == Devices::DeviceStatus::Offline) {
        return Tags::TagQuality::Offline;
    }

    return errorCode == 0 ? Tags::TagQuality::Good : Tags::TagQuality::DeviceError;
}

Tags::TagQuality TagValidationRule::validateRange(double value, const Tags::TagDefinition &definition)
{
    if (definition.minValue.has_value() && value < definition.minValue.value()) {
        return Tags::TagQuality::OutOfRange;
    }

    if (definition.maxValue.has_value() && value > definition.maxValue.value()) {
        return Tags::TagQuality::OutOfRange;
    }

    return Tags::TagQuality::Good;
}

Tags::TagAlarmState AlarmRule::evaluate(
    double value,
    Tags::TagQuality quality,
    const Alarms::AlarmDefinition &definition)
{
    if (quality == Tags::TagQuality::Offline) {
        return Tags::TagAlarmState::Offline;
    }

    if (quality != Tags::TagQuality::Good) {
        return Tags::TagAlarmState::Invalid;
    }

    if (definition.alarmHigh.has_value() && value >= definition.alarmHigh.value()) {
        return Tags::TagAlarmState::AlarmHigh;
    }

    if (definition.alarmLow.has_value() && value <= definition.alarmLow.value()) {
        return Tags::TagAlarmState::AlarmLow;
    }

    if (definition.warningHigh.has_value() && value >= definition.warningHigh.value()) {
        return Tags::TagAlarmState::WarningHigh;
    }

    if (definition.warningLow.has_value() && value <= definition.warningLow.value()) {
        return Tags::TagAlarmState::WarningLow;
    }

    return Tags::TagAlarmState::Normal;
}

} // namespace Monitor::Domain::Rules
