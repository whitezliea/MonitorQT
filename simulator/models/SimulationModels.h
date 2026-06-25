#ifndef SIMULATIONMODELS_H
#define SIMULATIONMODELS_H

#include "domain/devices/DeviceModels.h"
#include "domain/tags/TagModels.h"

#include <QDateTime>
#include <QString>

#include <optional>

namespace Monitor::Simulator::Models {

struct ChannelSimulationSpec
{
    QString code;
    QString unit;
    double baseValue = 0.0;
    double physicalMin = 0.0;
    double physicalMax = 0.0;
    double noiseSigma = 0.0;
    double sineAmplitude = 0.0;
    double sinePeriodSeconds = 1.0;
    double driftPerSecond = 0.0;
    double driftMin = 0.0;
    double driftMax = 0.0;
};

class ChannelSimulationState
{
public:
    explicit ChannelSimulationState(double phase = 0.0);

    double drift() const;
    double phase() const;
    void advanceDrift(double driftPerSecond, double deltaSeconds, double minimum, double maximum);

private:
    double m_drift = 0.0;
    double m_phase = 0.0;
};

struct MatrixSimulationSpec
{
    int rows = 16;
    int columns = 16;
    QString valueType = QStringLiteral("LightIntensity");
    QString unit = QStringLiteral("lux");
    double baseValue = 520.0;
    double centerAmplitude = 120.0;
    double edgeDrop = 85.0;
    double noiseSigma = 4.0;
};

class SimulationClock
{
public:
    explicit SimulationClock(const QDateTime &startTimeUtc);

    QDateTime startTimeUtc() const;
    QDateTime lastFrameTimeUtc() const;
    QPair<double, double> advance(const QDateTime &timestampUtc);

private:
    QDateTime m_startTimeUtc;
    QDateTime m_lastFrameTimeUtc;
};

struct DeviceEffect
{
    std::optional<Monitor::Domain::Devices::DeviceStatus> forcedStatus;
    std::optional<Monitor::Domain::Tags::TagQuality> forcedFrameQuality;
    int errorCode = 0;
    bool suppressFrame = false;
};

struct ChannelEffect
{
    double offset = 0.0;
    double scale = 1.0;
    std::optional<double> overrideValue;
    std::optional<Monitor::Domain::Tags::TagQuality> forcedQuality;
    int errorCode = 0;
    bool missingValue = false;
};

struct MatrixEffect
{
    bool addHotspot = false;
    int hotspotRow = 8;
    int hotspotColumn = 8;
    double hotspotAmplitude = 0.0;
    bool addLowRegion = false;
    int lowRegionRow = 4;
    int lowRegionColumn = 4;
    double lowRegionScale = 1.0;
    std::optional<Monitor::Domain::Tags::TagQuality> forcedQuality;
};

} // namespace Monitor::Simulator::Models

#endif // SIMULATIONMODELS_H
