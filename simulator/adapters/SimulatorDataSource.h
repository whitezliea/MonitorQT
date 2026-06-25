#ifndef SIMULATORDATASOURCE_H
#define SIMULATORDATASOURCE_H

#include "application/abstractions/IRawFrameSource.h"
#include "application/configuration/MonitorRuntimeOptions.h"
#include "simulator/generators/FakeDataGenerator.h"

#include <QDateTime>

namespace Monitor::Simulator::Adapters {

class SimulatorDataSource : public Monitor::Application::Abstractions::IRawFrameSource
{
public:
    SimulatorDataSource();
    SimulatorDataSource(
        const Monitor::Simulator::Generators::FakeDataGenerator &generator,
        const Monitor::Application::Configuration::MonitorRuntimeOptions &options);

    Monitor::Domain::Measurements::RawMeasurementFrame readNextFrame(const QDateTime &timestampUtc);
    bool readNextFrame(
        const QDateTime &timestampUtc,
        Monitor::Domain::Measurements::RawMeasurementFrame *frame) override;
    void cancel() override;
    void resetCancellation() override;
    bool isCanceled() const;
    int dataGenerateIntervalMs() const;
    Monitor::Simulator::Generators::FakeDataGenerator &generator();
    const Monitor::Simulator::Generators::FakeDataGenerator &generator() const;

private:
    Monitor::Simulator::Generators::FakeDataGenerator m_generator;
    Monitor::Application::Configuration::MonitorRuntimeOptions m_options;
    bool m_canceled = false;
};

} // namespace Monitor::Simulator::Adapters

#endif // SIMULATORDATASOURCE_H
