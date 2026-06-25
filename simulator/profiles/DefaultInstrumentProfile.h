#ifndef DEFAULTINSTRUMENTPROFILE_H
#define DEFAULTINSTRUMENTPROFILE_H

#include "simulator/models/SimulationModels.h"

#include <QVector>

namespace Monitor::Simulator::Profiles {

class DefaultInstrumentProfile
{
public:
    static QVector<Monitor::Simulator::Models::ChannelSimulationSpec> createChannels();
    static Monitor::Simulator::Models::MatrixSimulationSpec createMatrix();
};

} // namespace Monitor::Simulator::Profiles

#endif // DEFAULTINSTRUMENTPROFILE_H
