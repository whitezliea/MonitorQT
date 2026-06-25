#ifndef DEVICESTATUSGENERATOR_H
#define DEVICESTATUSGENERATOR_H

#include "domain/devices/DeviceModels.h"

namespace Monitor::Simulator::Generators {

class DeviceStatusGenerator
{
public:
    Monitor::Domain::Devices::DeviceStatus generate(qint64 sequenceNo) const;
};

class ErrorCodeGenerator
{
public:
    int generate(qint64 sequenceNo) const;
};

} // namespace Monitor::Simulator::Generators

#endif // DEVICESTATUSGENERATOR_H
