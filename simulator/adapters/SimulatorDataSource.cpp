#include "SimulatorDataSource.h"

#include <stdexcept>

namespace Monitor::Simulator::Adapters {

SimulatorDataSource::SimulatorDataSource()
    : SimulatorDataSource(
          Monitor::Simulator::Generators::FakeDataGenerator(),
          Monitor::Application::Configuration::MonitorRuntimeOptions())
{
}

SimulatorDataSource::SimulatorDataSource(
    const Monitor::Simulator::Generators::FakeDataGenerator &generator,
    const Monitor::Application::Configuration::MonitorRuntimeOptions &options)
    : m_generator(generator)
    , m_options(options)
{
}

Monitor::Domain::Measurements::RawMeasurementFrame SimulatorDataSource::readNextFrame(const QDateTime &timestampUtc)
{
    if (m_canceled) {
        throw std::runtime_error("Simulator data source has been canceled.");
    }

    return m_generator.nextFrame(timestampUtc);
}

void SimulatorDataSource::cancel()
{
    m_canceled = true;
}

void SimulatorDataSource::resetCancellation()
{
    m_canceled = false;
}

bool SimulatorDataSource::isCanceled() const
{
    return m_canceled;
}

int SimulatorDataSource::dataGenerateIntervalMs() const
{
    return m_options.dataGenerateIntervalMs;
}

Monitor::Simulator::Generators::FakeDataGenerator &SimulatorDataSource::generator()
{
    return m_generator;
}

const Monitor::Simulator::Generators::FakeDataGenerator &SimulatorDataSource::generator() const
{
    return m_generator;
}

} // namespace Monitor::Simulator::Adapters
