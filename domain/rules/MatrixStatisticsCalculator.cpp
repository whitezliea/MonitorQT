#include "MatrixStatisticsCalculator.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Monitor::Domain::Rules {
namespace {

constexpr double Epsilon = 1e-9;

} // namespace

Measurements::MatrixStatistics MatrixStatisticsCalculator::calculate(const QVector<double> &values)
{
    auto minValue = std::numeric_limits<double>::infinity();
    auto maxValue = -std::numeric_limits<double>::infinity();
    auto mean = 0.0;
    auto sumOfSquaredDifferences = 0.0;
    auto validCount = 0;
    auto invalidCount = 0;

    for (const auto value : values) {
        if (!std::isfinite(value)) {
            ++invalidCount;
            continue;
        }

        minValue = std::min(minValue, value);
        maxValue = std::max(maxValue, value);

        ++validCount;
        const auto delta = value - mean;
        mean += delta / validCount;
        const auto deltaFromUpdatedMean = value - mean;
        sumOfSquaredDifferences += delta * deltaFromUpdatedMean;
    }

    if (validCount == 0) {
        const auto nan = std::numeric_limits<double>::quiet_NaN();
        return {
            nan,
            nan,
            nan,
            nan,
            nan,
            nan,
            0,
            invalidCount
        };
    }

    const auto variance = sumOfSquaredDifferences / validCount;
    const auto stdDev = std::sqrt(std::max(0.0, variance));
    const auto uniformityMinMax = std::abs(maxValue) < Epsilon
        ? std::numeric_limits<double>::quiet_NaN()
        : minValue / maxValue;
    const auto uniformityMinAverage = std::abs(mean) < Epsilon
        ? std::numeric_limits<double>::quiet_NaN()
        : minValue / mean;

    return {
        minValue,
        maxValue,
        mean,
        stdDev,
        uniformityMinMax,
        uniformityMinAverage,
        validCount,
        invalidCount
    };
}

Measurements::MatrixStatistics MatrixStatisticsRule::calculate(const Measurements::MatrixFrame &frame)
{
    return frame.calculateStatistics();
}

} // namespace Monitor::Domain::Rules
