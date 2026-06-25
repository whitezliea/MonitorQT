#include "DeviceModels.h"

namespace Monitor::Domain::Devices {

QString toString(DeviceStatus status)
{
    switch (status) {
    case DeviceStatus::Stopped:
        return QStringLiteral("Stopped");
    case DeviceStatus::Running:
        return QStringLiteral("Running");
    case DeviceStatus::Warning:
        return QStringLiteral("Warning");
    case DeviceStatus::Error:
        return QStringLiteral("Error");
    case DeviceStatus::Offline:
        return QStringLiteral("Offline");
    }

    return QString();
}

QString toString(DeviceConnectionState state)
{
    switch (state) {
    case DeviceConnectionState::Connected:
        return QStringLiteral("Connected");
    case DeviceConnectionState::Disconnected:
        return QStringLiteral("Disconnected");
    case DeviceConnectionState::Timeout:
        return QStringLiteral("Timeout");
    }

    return QString();
}

} // namespace Monitor::Domain::Devices
