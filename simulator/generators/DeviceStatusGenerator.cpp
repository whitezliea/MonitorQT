#include "DeviceStatusGenerator.h"

namespace Monitor::Simulator::Generators {

Monitor::Domain::Devices::DeviceStatus DeviceStatusGenerator::generate(qint64 sequenceNo) const
{
    return sequenceNo % 120 >= 110
        ? Monitor::Domain::Devices::DeviceStatus::Offline
        : Monitor::Domain::Devices::DeviceStatus::Running;
}

int ErrorCodeGenerator::generate(qint64 sequenceNo) const
{
    return sequenceNo % 150 == 0 ? 1 : 0;
}

} // namespace Monitor::Simulator::Generators
