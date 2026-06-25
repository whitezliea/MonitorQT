#ifndef FAKEDATAGENERATOR_H
#define FAKEDATAGENERATOR_H

#include "ChannelValueGenerator.h"
#include "MatrixValueGenerator.h"

#include "domain/measurements/MeasurementModels.h"
#include "simulator/models/SimulationModels.h"
#include "simulator/scenarios/SimulationScenario.h"

namespace Monitor::Simulator::Generators {

class FakeDataGenerator
{
public:
    FakeDataGenerator();
    FakeDataGenerator(
        const QString &deviceId,
        const Monitor::Simulator::Scenarios::SimulationScenario &scenario,
        const QDateTime &startTimeUtc);
    FakeDataGenerator(
        const QString &deviceId,
        const Monitor::Simulator::Scenarios::SimulationScenario &scenario,
        const QVector<Monitor::Simulator::Models::ChannelSimulationSpec> &channelSpecs,
        const Monitor::Simulator::Models::MatrixSimulationSpec &matrixSpec,
        const QDateTime &startTimeUtc);

    Monitor::Domain::Measurements::RawMeasurementFrame nextFrame(const QDateTime &timestampUtc);
    qint64 sequenceNo() const;
    QString deviceId() const;
    Monitor::Simulator::Scenarios::SimulationScenario scenario() const;

private:
    static Monitor::Domain::Tags::TagQuality resolveFrameQuality(
        const QVector<Monitor::Domain::Measurements::ChannelValue> &channels,
        const Monitor::Simulator::Models::DeviceEffect &deviceEffect);
    static Monitor::Domain::Devices::DeviceStatus resolveDeviceStatus(Monitor::Domain::Tags::TagQuality quality);

    QString m_deviceId;
    QVector<Monitor::Simulator::Models::ChannelSimulationSpec> m_channelSpecs;
    Monitor::Simulator::Models::MatrixSimulationSpec m_matrixSpec;
    ChannelValueGenerator m_channelGenerator;
    MatrixValueGenerator m_matrixGenerator;
    Monitor::Simulator::Scenarios::SimulationScenario m_scenario;
    Monitor::Simulator::Models::SimulationClock m_clock;
    qint64 m_sequenceNo = 0;
};

} // namespace Monitor::Simulator::Generators

#endif // FAKEDATAGENERATOR_H
