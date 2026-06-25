#ifndef SIMULATIONSCENARIO_H
#define SIMULATIONSCENARIO_H

#include "simulator/models/SimulationModels.h"

#include <QString>

namespace Monitor::Simulator::Scenarios {

enum class ScenarioKind {
    Demo,
    Normal,
    Offline,
    MatrixHotspot,
    Alarm
};

class SimulationScenario
{
public:
    explicit SimulationScenario(ScenarioKind kind = ScenarioKind::Demo);

    ScenarioKind kind() const;
    QString name() const;
    Models::DeviceEffect deviceEffect(double elapsedSeconds, qint64 sequenceNo) const;
    Models::ChannelEffect channelEffect(const QString &channelCode, double elapsedSeconds, qint64 sequenceNo) const;
    Models::MatrixEffect matrixEffect(double elapsedSeconds, qint64 sequenceNo) const;

    static SimulationScenario demo();
    static SimulationScenario normal();
    static SimulationScenario offline();
    static SimulationScenario matrixHotspot();
    static SimulationScenario alarm();

private:
    double cycleSeconds(double elapsedSeconds) const;

    ScenarioKind m_kind = ScenarioKind::Demo;
};

} // namespace Monitor::Simulator::Scenarios

#endif // SIMULATIONSCENARIO_H
