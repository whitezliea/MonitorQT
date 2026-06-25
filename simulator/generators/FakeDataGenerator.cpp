#include "FakeDataGenerator.h"

#include "application/services/TagDefinitionCatalog.h"
#include "domain/common/DomainCommon.h"
#include "simulator/profiles/DefaultInstrumentProfile.h"

#include <algorithm>

namespace Monitor::Simulator::Generators {

FakeDataGenerator::FakeDataGenerator()
    : FakeDataGenerator(
          Monitor::Application::Services::TagDefinitionCatalog::defaultDeviceId(),
          Monitor::Simulator::Scenarios::SimulationScenario::demo(),
          QDateTime::currentDateTimeUtc())
{
}

FakeDataGenerator::FakeDataGenerator(
    const QString &deviceId,
    const Monitor::Simulator::Scenarios::SimulationScenario &scenario,
    const QDateTime &startTimeUtc)
    : FakeDataGenerator(
          deviceId,
          scenario,
          Monitor::Simulator::Profiles::DefaultInstrumentProfile::createChannels(),
          Monitor::Simulator::Profiles::DefaultInstrumentProfile::createMatrix(),
          startTimeUtc)
{
}

FakeDataGenerator::FakeDataGenerator(
    const QString &deviceId,
    const Monitor::Simulator::Scenarios::SimulationScenario &scenario,
    const QVector<Monitor::Simulator::Models::ChannelSimulationSpec> &channelSpecs,
    const Monitor::Simulator::Models::MatrixSimulationSpec &matrixSpec,
    const QDateTime &startTimeUtc)
    : m_deviceId(deviceId)
    , m_channelSpecs(channelSpecs)
    , m_matrixSpec(matrixSpec)
    , m_scenario(scenario)
    , m_clock(startTimeUtc)
{
}

Monitor::Domain::Measurements::RawMeasurementFrame FakeDataGenerator::nextFrame(const QDateTime &timestampUtc)
{
    const auto utc = Monitor::Domain::Common::UtcDateTime::require(timestampUtc, QStringLiteral("timestampUtc"));
    ++m_sequenceNo;
    const auto timing = m_clock.advance(utc);
    const auto elapsedSeconds = timing.first;
    const auto deltaSeconds = timing.second;
    const auto deviceEffect = m_scenario.deviceEffect(elapsedSeconds, m_sequenceNo);

    QVector<Monitor::Domain::Measurements::ChannelValue> channels;
    channels.reserve(m_channelSpecs.size());
    for (const auto &spec : m_channelSpecs) {
        const auto effect = m_scenario.channelEffect(spec.code, elapsedSeconds, m_sequenceNo);
        channels.append(m_channelGenerator.generate(spec, elapsedSeconds, deltaSeconds, effect));
    }

    const auto matrixEffect = m_scenario.matrixEffect(elapsedSeconds, m_sequenceNo);
    auto matrix = m_matrixGenerator.generate(m_matrixSpec, utc, elapsedSeconds, matrixEffect);
    const auto quality = resolveFrameQuality(channels, deviceEffect);
    const auto status = deviceEffect.forcedStatus.value_or(resolveDeviceStatus(quality));

    Monitor::Domain::Measurements::RawMeasurementFrame frame;
    frame.frameId = QUuid::createUuid();
    frame.deviceId = m_deviceId;
    frame.sequenceNo = m_sequenceNo;
    frame.timestampUtc = utc;
    frame.deviceStatus = status;
    frame.channelValues = channels;
    matrix.sourceFrameId = frame.frameId;
    matrix.sequenceNo = frame.sequenceNo;
    frame.matrixValues = matrix;
    frame.errorCode = deviceEffect.errorCode;
    frame.quality = quality;
    return frame;
}

qint64 FakeDataGenerator::sequenceNo() const
{
    return m_sequenceNo;
}

QString FakeDataGenerator::deviceId() const
{
    return m_deviceId;
}

Monitor::Simulator::Scenarios::SimulationScenario FakeDataGenerator::scenario() const
{
    return m_scenario;
}

Monitor::Domain::Tags::TagQuality FakeDataGenerator::resolveFrameQuality(
    const QVector<Monitor::Domain::Measurements::ChannelValue> &channels,
    const Monitor::Simulator::Models::DeviceEffect &deviceEffect)
{
    using Monitor::Domain::Tags::TagQuality;

    if (deviceEffect.forcedFrameQuality.has_value()) {
        return deviceEffect.forcedFrameQuality.value();
    }

    if (std::all_of(channels.cbegin(), channels.cend(), [](const auto &channel) {
            return channel.quality == TagQuality::Good;
        })) {
        return TagQuality::Good;
    }

    if (std::any_of(channels.cbegin(), channels.cend(), [](const auto &channel) {
            return channel.quality == TagQuality::Offline;
        })) {
        return TagQuality::Offline;
    }

    if (std::any_of(channels.cbegin(), channels.cend(), [](const auto &channel) {
            return channel.quality == TagQuality::Timeout;
        })) {
        return TagQuality::Timeout;
    }

    if (std::any_of(channels.cbegin(), channels.cend(), [](const auto &channel) {
            return channel.quality == TagQuality::DeviceError;
        })) {
        return TagQuality::DeviceError;
    }

    if (std::any_of(channels.cbegin(), channels.cend(), [](const auto &channel) {
            return channel.quality == TagQuality::OutOfRange;
        })) {
        return TagQuality::OutOfRange;
    }

    return TagQuality::Bad;
}

Monitor::Domain::Devices::DeviceStatus FakeDataGenerator::resolveDeviceStatus(Monitor::Domain::Tags::TagQuality quality)
{
    using Monitor::Domain::Devices::DeviceStatus;
    using Monitor::Domain::Tags::TagQuality;

    switch (quality) {
    case TagQuality::Good:
        return DeviceStatus::Running;
    case TagQuality::DeviceError:
        return DeviceStatus::Error;
    case TagQuality::Offline:
        return DeviceStatus::Offline;
    case TagQuality::Bad:
    case TagQuality::Timeout:
    case TagQuality::OutOfRange:
        return DeviceStatus::Warning;
    }

    return DeviceStatus::Warning;
}

} // namespace Monitor::Simulator::Generators
