#ifndef SIMULATIONPROFILE_H
#define SIMULATIONPROFILE_H

#include <QString>

namespace Monitor::Simulator::Profiles {

struct SimulationProfile
{
    QString name;
    bool enableTemperatureDrift = true;
    bool enableVibrationSpike = true;
    bool enableOfflineWindow = true;
    bool enableMatrixHotspot = true;
};

class DefaultProfiles
{
public:
    static SimulationProfile demo();
    static SimulationProfile normal();
    static SimulationProfile alarm();
};

} // namespace Monitor::Simulator::Profiles

#endif // SIMULATIONPROFILE_H
