#include "DefaultInstrumentProfile.h"

namespace Monitor::Simulator::Profiles {

QVector<Monitor::Simulator::Models::ChannelSimulationSpec> DefaultInstrumentProfile::createChannels()
{
    using Monitor::Simulator::Models::ChannelSimulationSpec;

    return {
        ChannelSimulationSpec{QStringLiteral("TEMP_CH01"), QStringLiteral("C"), 25.0, -20.0, 120.0, 0.08, 0.25, 60.0, 0.001, -1.5, 8.0},
        ChannelSimulationSpec{QStringLiteral("PRESSURE_CH01"), QStringLiteral("kPa"), 101.3, 80.0, 130.0, 0.12, 0.35, 45.0, 0.0002, -2.0, 2.0},
        ChannelSimulationSpec{QStringLiteral("LIGHT_CH01"), QStringLiteral("lux"), 580.0, 0.0, 2000.0, 2.5, 25.0, 20.0, 0.02, -80.0, 120.0},
        ChannelSimulationSpec{QStringLiteral("VOLTAGE_CH01"), QStringLiteral("V"), 12.0, 0.0, 30.0, 0.03, 0.04, 15.0, 0.0, -0.5, 0.5},
        ChannelSimulationSpec{QStringLiteral("CURRENT_CH01"), QStringLiteral("A"), 1.2, 0.0, 5.0, 0.02, 0.08, 18.0, 0.0, -0.2, 0.4},
        ChannelSimulationSpec{QStringLiteral("VIBRATION_CH01"), QStringLiteral("mm/s"), 0.03, 0.0, 10.0, 0.01, 0.01, 8.0, 0.0, 0.0, 0.2}
    };
}

Monitor::Simulator::Models::MatrixSimulationSpec DefaultInstrumentProfile::createMatrix()
{
    return {16, 16, QStringLiteral("LightIntensity"), QStringLiteral("lux"), 520.0, 120.0, 85.0, 4.0};
}

} // namespace Monitor::Simulator::Profiles
