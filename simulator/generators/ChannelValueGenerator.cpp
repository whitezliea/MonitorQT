#include "ChannelValueGenerator.h"

#include "simulator/noise/NoiseModels.h"

#include <cmath>
#include <limits>

namespace Monitor::Simulator::Generators {
namespace {

constexpr double Pi = 3.14159265358979323846;

} // namespace

ChannelValueGenerator::ChannelValueGenerator(quint32 seed)
    : m_random(seed)
{
}

Monitor::Domain::Measurements::ChannelValue ChannelValueGenerator::generate(
    const Monitor::Simulator::Models::ChannelSimulationSpec &spec,
    double elapsedSeconds,
    double deltaSeconds,
    const Monitor::Simulator::Models::ChannelEffect &effect)
{
    auto &state = stateFor(spec.code);
    state.advanceDrift(spec.driftPerSecond, deltaSeconds, spec.driftMin, spec.driftMax);

    if (effect.missingValue) {
        return {
            spec.code,
            std::numeric_limits<double>::quiet_NaN(),
            spec.unit,
            effect.forcedQuality.value_or(Monitor::Domain::Tags::TagQuality::Bad),
            effect.errorCode
        };
    }

    auto value = spec.baseValue
        + Monitor::Simulator::Noise::sineWave(elapsedSeconds, spec.sineAmplitude, spec.sinePeriodSeconds, state.phase())
        + state.drift()
        + Monitor::Simulator::Noise::gaussian(m_random, 0.0, spec.noiseSigma);
    value = value * effect.scale + effect.offset;

    if (effect.overrideValue.has_value()) {
        value = effect.overrideValue.value();
    }

    const auto quality = effect.forcedQuality.value_or(evaluateQuality(value, spec));
    return {
        spec.code,
        Monitor::Simulator::Noise::roundTo(value, 4),
        spec.unit,
        quality,
        effect.errorCode
    };
}

Monitor::Simulator::Models::ChannelSimulationState &ChannelValueGenerator::stateFor(const QString &code)
{
    auto it = m_states.find(code);
    if (it != m_states.end()) {
        return it.value();
    }

    std::uniform_real_distribution<double> distribution(0.0, 2.0 * Pi);
    auto inserted = m_states.insert(code, Monitor::Simulator::Models::ChannelSimulationState(distribution(m_random)));
    return inserted.value();
}

Monitor::Domain::Tags::TagQuality ChannelValueGenerator::evaluateQuality(
    double value,
    const Monitor::Simulator::Models::ChannelSimulationSpec &spec)
{
    if (value < spec.physicalMin || value > spec.physicalMax) {
        return Monitor::Domain::Tags::TagQuality::OutOfRange;
    }

    return Monitor::Domain::Tags::TagQuality::Good;
}

} // namespace Monitor::Simulator::Generators
