#include "SimulationScenario.h"

#include <cmath>

namespace Monitor::Simulator::Scenarios {
namespace {

constexpr double CycleSeconds = 120.0;

bool inWindow(double t, double start, double end)
{
    return t >= start && t < end;
}

} // namespace

SimulationScenario::SimulationScenario(ScenarioKind kind)
    : m_kind(kind)
{
}

ScenarioKind SimulationScenario::kind() const
{
    return m_kind;
}

QString SimulationScenario::name() const
{
    switch (m_kind) {
    case ScenarioKind::Demo:
        return QStringLiteral("Interview Demo");
    case ScenarioKind::Normal:
        return QStringLiteral("Normal");
    case ScenarioKind::Offline:
        return QStringLiteral("Offline");
    case ScenarioKind::MatrixHotspot:
        return QStringLiteral("Matrix Hotspot");
    case ScenarioKind::Alarm:
        return QStringLiteral("Alarm");
    }

    return QString();
}

Models::DeviceEffect SimulationScenario::deviceEffect(double elapsedSeconds, qint64 sequenceNo) const
{
    Q_UNUSED(sequenceNo)
    using Monitor::Domain::Devices::DeviceStatus;
    using Monitor::Domain::Tags::TagQuality;

    if (m_kind == ScenarioKind::Offline) {
        return {DeviceStatus::Offline, TagQuality::Offline, 1001, false};
    }

    if (m_kind == ScenarioKind::Alarm || m_kind == ScenarioKind::Normal || m_kind == ScenarioKind::MatrixHotspot) {
        return {DeviceStatus::Running, std::nullopt, 0, false};
    }

    const auto t = cycleSeconds(elapsedSeconds);
    if (inWindow(t, 90.0, 96.0)) {
        return {DeviceStatus::Offline, TagQuality::Offline, 1001, false};
    }

    if (inWindow(t, 70.0, 75.0)) {
        return {DeviceStatus::Error, TagQuality::DeviceError, 2001, false};
    }

    return {DeviceStatus::Running, std::nullopt, 0, false};
}

Models::ChannelEffect SimulationScenario::channelEffect(const QString &channelCode, double elapsedSeconds, qint64 sequenceNo) const
{
    Q_UNUSED(sequenceNo)
    using Monitor::Domain::Tags::TagQuality;

    if (m_kind == ScenarioKind::Offline) {
        return {0.0, 1.0, std::nullopt, TagQuality::Offline, 1001, true};
    }

    if (m_kind == ScenarioKind::Alarm) {
        return channelCode == QStringLiteral("TEMP_CH01")
            ? Models::ChannelEffect{60.0, 1.0, std::nullopt, std::nullopt, 0, false}
            : Models::ChannelEffect{};
    }

    if (m_kind == ScenarioKind::Normal || m_kind == ScenarioKind::MatrixHotspot) {
        return {};
    }

    const auto t = cycleSeconds(elapsedSeconds);
    if (channelCode == QStringLiteral("TEMP_CH01") && inWindow(t, 20.0, 35.0)) {
        return {(t - 20.0) * 1.2, 1.0, std::nullopt, std::nullopt, 0, false};
    }

    if (channelCode == QStringLiteral("VIBRATION_CH01") && inWindow(t, 40.0, 42.0)) {
        return {0.0, 1.0, 3.5, std::nullopt, 0, false};
    }

    if (channelCode == QStringLiteral("VOLTAGE_CH01") && inWindow(t, 55.0, 60.0)) {
        return {-2.8, 1.0, std::nullopt, std::nullopt, 0, false};
    }

    if (channelCode == QStringLiteral("LIGHT_CH01") && inWindow(t, 65.0, 68.0)) {
        return {0.0, 1.0, std::nullopt, TagQuality::DeviceError, 3001, true};
    }

    if (inWindow(t, 90.0, 96.0)) {
        return {0.0, 1.0, std::nullopt, TagQuality::Offline, 1001, true};
    }

    return {};
}

Models::MatrixEffect SimulationScenario::matrixEffect(double elapsedSeconds, qint64 sequenceNo) const
{
    Q_UNUSED(sequenceNo)
    using Monitor::Domain::Tags::TagQuality;

    if (m_kind == ScenarioKind::Offline) {
        return {false, 8, 8, 0.0, false, 4, 4, 1.0, TagQuality::Offline};
    }

    if (m_kind == ScenarioKind::MatrixHotspot) {
        return {true, 9, 10, 350.0, false, 4, 4, 1.0, std::nullopt};
    }

    if (m_kind == ScenarioKind::Alarm || m_kind == ScenarioKind::Normal) {
        return {};
    }

    const auto t = cycleSeconds(elapsedSeconds);
    if (inWindow(t, 80.0, 88.0)) {
        return {true, 9, 10, 350.0, false, 4, 4, 1.0, std::nullopt};
    }

    if (inWindow(t, 100.0, 108.0)) {
        return {false, 8, 8, 0.0, true, 5, 5, 0.55, std::nullopt};
    }

    return {};
}

SimulationScenario SimulationScenario::demo()
{
    return SimulationScenario(ScenarioKind::Demo);
}

SimulationScenario SimulationScenario::normal()
{
    return SimulationScenario(ScenarioKind::Normal);
}

SimulationScenario SimulationScenario::offline()
{
    return SimulationScenario(ScenarioKind::Offline);
}

SimulationScenario SimulationScenario::matrixHotspot()
{
    return SimulationScenario(ScenarioKind::MatrixHotspot);
}

SimulationScenario SimulationScenario::alarm()
{
    return SimulationScenario(ScenarioKind::Alarm);
}

double SimulationScenario::cycleSeconds(double elapsedSeconds) const
{
    auto t = std::fmod(elapsedSeconds, CycleSeconds);
    if (t < 0.0) {
        t += CycleSeconds;
    }
    return t;
}

} // namespace Monitor::Simulator::Scenarios
