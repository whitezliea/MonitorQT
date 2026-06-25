#ifndef DOMAINRULES_H
#define DOMAINRULES_H

#include "domain/alarms/AlarmModels.h"
#include "domain/devices/DeviceModels.h"
#include "domain/tags/TagModels.h"

namespace Monitor::Domain::Rules {

class QualityRule
{
public:
    static Tags::TagQuality fromDeviceState(Devices::DeviceStatus status, int errorCode);
};

class TagValidationRule
{
public:
    static Tags::TagQuality validateRange(double value, const Tags::TagDefinition &definition);
};

class AlarmRule
{
public:
    static Tags::TagAlarmState evaluate(
        double value,
        Tags::TagQuality quality,
        const Alarms::AlarmDefinition &definition);
};

} // namespace Monitor::Domain::Rules

#endif // DOMAINRULES_H
