#ifndef DEVICEMODELS_H
#define DEVICEMODELS_H

#include <QString>

namespace Monitor::Domain::Devices {

enum class DeviceConnectionState {
    Connected,
    Disconnected,
    Timeout
};

enum class DeviceErrorCode {
    None = 0,
    GeneralError = 1,
    SensorFault = 2,
    CommunicationTimeout = 3
};

enum class DeviceStatus {
    Stopped,
    Running,
    Warning,
    Error,
    Offline
};

struct DeviceInfo
{
    QString deviceId;
    QString displayName;
    QString model;
};

struct SamplingConfig
{
    int dataGenerateIntervalMs = 0;
    int matrixGenerateIntervalMs = 0;
};

QString toString(DeviceStatus status);
QString toString(DeviceConnectionState state);

} // namespace Monitor::Domain::Devices

#endif // DEVICEMODELS_H
