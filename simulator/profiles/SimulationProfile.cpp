#include "SimulationProfile.h"

namespace Monitor::Simulator::Profiles {

SimulationProfile DefaultProfiles::demo()
{
    return {QStringLiteral("Demo"), true, true, true, true};
}

SimulationProfile DefaultProfiles::normal()
{
    return {QStringLiteral("Normal"), false, false, false, false};
}

SimulationProfile DefaultProfiles::alarm()
{
    return {QStringLiteral("Alarm"), true, true, false, true};
}

} // namespace Monitor::Simulator::Profiles
