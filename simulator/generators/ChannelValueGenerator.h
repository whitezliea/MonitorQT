#ifndef CHANNELVALUEGENERATOR_H
#define CHANNELVALUEGENERATOR_H

#include "domain/measurements/MeasurementModels.h"
#include "simulator/models/SimulationModels.h"

#include <QHash>

#include <random>

namespace Monitor::Simulator::Generators {

class ChannelValueGenerator
{
public:
    explicit ChannelValueGenerator(quint32 seed = 20260607);

    Monitor::Domain::Measurements::ChannelValue generate(
        const Monitor::Simulator::Models::ChannelSimulationSpec &spec,
        double elapsedSeconds,
        double deltaSeconds,
        const Monitor::Simulator::Models::ChannelEffect &effect);

private:
    Monitor::Simulator::Models::ChannelSimulationState &stateFor(const QString &code);
    static Monitor::Domain::Tags::TagQuality evaluateQuality(
        double value,
        const Monitor::Simulator::Models::ChannelSimulationSpec &spec);

    std::mt19937 m_random;
    QHash<QString, Monitor::Simulator::Models::ChannelSimulationState> m_states;
};

} // namespace Monitor::Simulator::Generators

#endif // CHANNELVALUEGENERATOR_H
