#include "SimulationModels.h"

#include "domain/common/DomainCommon.h"

#include <algorithm>

namespace Monitor::Simulator::Models {

ChannelSimulationState::ChannelSimulationState(double phase)
    : m_phase(phase)
{
}

double ChannelSimulationState::drift() const
{
    return m_drift;
}

double ChannelSimulationState::phase() const
{
    return m_phase;
}

void ChannelSimulationState::advanceDrift(double driftPerSecond, double deltaSeconds, double minimum, double maximum)
{
    m_drift += driftPerSecond * deltaSeconds;
    m_drift = std::clamp(m_drift, minimum, maximum);
}

SimulationClock::SimulationClock(const QDateTime &startTimeUtc)
    : m_startTimeUtc(Monitor::Domain::Common::UtcDateTime::require(startTimeUtc, QStringLiteral("startTimeUtc")))
    , m_lastFrameTimeUtc(m_startTimeUtc)
{
}

QDateTime SimulationClock::startTimeUtc() const
{
    return m_startTimeUtc;
}

QDateTime SimulationClock::lastFrameTimeUtc() const
{
    return m_lastFrameTimeUtc;
}

QPair<double, double> SimulationClock::advance(const QDateTime &timestampUtc)
{
    const auto utc = Monitor::Domain::Common::UtcDateTime::require(timestampUtc, QStringLiteral("timestampUtc"));
    const auto elapsedSeconds = m_startTimeUtc.msecsTo(utc) / 1000.0;
    const auto deltaSeconds = std::max(0.001, m_lastFrameTimeUtc.msecsTo(utc) / 1000.0);
    m_lastFrameTimeUtc = utc;
    return {elapsedSeconds, deltaSeconds};
}

} // namespace Monitor::Simulator::Models
