#ifndef MATRIXVALUEGENERATOR_H
#define MATRIXVALUEGENERATOR_H

#include "domain/measurements/MeasurementModels.h"
#include "simulator/models/SimulationModels.h"

#include <random>

namespace Monitor::Simulator::Generators {

class MatrixValueGenerator
{
public:
    explicit MatrixValueGenerator(quint32 seed = 20260608);

    Monitor::Domain::Measurements::MatrixFrame generate(
        const Monitor::Simulator::Models::MatrixSimulationSpec &spec,
        const QDateTime &timestampUtc,
        double elapsedSeconds,
        const Monitor::Simulator::Models::MatrixEffect &effect);

private:
    static double distance(double row, double column, double centerRow, double centerColumn);
    static double gaussianWeight(int row, int column, int centerRow, int centerColumn, double sigma);
    static double gaussianBump(int row, int column, int centerRow, int centerColumn, double amplitude, double sigma);

    std::mt19937 m_random;
};

} // namespace Monitor::Simulator::Generators

#endif // MATRIXVALUEGENERATOR_H
