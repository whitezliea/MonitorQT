#include "MatrixValueGenerator.h"

#include "simulator/noise/NoiseModels.h"

#include <cmath>

namespace Monitor::Simulator::Generators {

MatrixValueGenerator::MatrixValueGenerator(quint32 seed)
    : m_random(seed)
{
}

Monitor::Domain::Measurements::MatrixFrame MatrixValueGenerator::generate(
    const Monitor::Simulator::Models::MatrixSimulationSpec &spec,
    const QDateTime &timestampUtc,
    double elapsedSeconds,
    const Monitor::Simulator::Models::MatrixEffect &effect)
{
    Monitor::Domain::Measurements::MatrixFrame frame;
    frame.frameId = QUuid::createUuid();
    frame.timestampUtc = timestampUtc.toUTC();
    frame.rows = spec.rows;
    frame.columns = spec.columns;
    frame.values.reserve(spec.rows * spec.columns);

    const auto centerRow = (spec.rows - 1) / 2.0;
    const auto centerColumn = (spec.columns - 1) / 2.0;
    const auto maxDistance = distance(0.0, 0.0, centerRow, centerColumn);
    const auto slowWave = std::sin(elapsedSeconds / 10.0) * 8.0;

    for (auto row = 0; row < spec.rows; ++row) {
        for (auto column = 0; column < spec.columns; ++column) {
            const auto cellDistance = distance(row, column, centerRow, centerColumn);
            const auto normalizedDistance = cellDistance / maxDistance;
            const auto centerGain = spec.centerAmplitude * std::exp(-std::pow(normalizedDistance, 2.0) * 2.5);
            const auto edgeDrop = spec.edgeDrop * normalizedDistance;
            auto value = spec.baseValue
                + centerGain
                - edgeDrop
                + slowWave
                + Monitor::Simulator::Noise::gaussian(m_random, 0.0, spec.noiseSigma);

            if (effect.addHotspot) {
                value += gaussianBump(row, column, effect.hotspotRow, effect.hotspotColumn, effect.hotspotAmplitude, 1.4);
            }

            if (effect.addLowRegion) {
                const auto weight = gaussianWeight(row, column, effect.lowRegionRow, effect.lowRegionColumn, 1.8);
                value *= 1.0 - weight * (1.0 - effect.lowRegionScale);
            }

            frame.values.append(Monitor::Simulator::Noise::roundTo(value, 3));
        }
    }

    return frame;
}

double MatrixValueGenerator::distance(double row, double column, double centerRow, double centerColumn)
{
    const auto rowDelta = row - centerRow;
    const auto columnDelta = column - centerColumn;
    return std::sqrt(rowDelta * rowDelta + columnDelta * columnDelta);
}

double MatrixValueGenerator::gaussianWeight(int row, int column, int centerRow, int centerColumn, double sigma)
{
    const auto rowDelta = row - centerRow;
    const auto columnDelta = column - centerColumn;
    const auto distanceSquared = rowDelta * rowDelta + columnDelta * columnDelta;
    return std::exp(-distanceSquared / (2.0 * sigma * sigma));
}

double MatrixValueGenerator::gaussianBump(int row, int column, int centerRow, int centerColumn, double amplitude, double sigma)
{
    return amplitude * gaussianWeight(row, column, centerRow, centerColumn, sigma);
}

} // namespace Monitor::Simulator::Generators
