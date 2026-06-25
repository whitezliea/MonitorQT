#include "NoiseModels.h"

#include <cmath>

namespace Monitor::Simulator::Noise {
namespace {

constexpr double Pi = 3.14159265358979323846;

} // namespace

double sineWave(double elapsedSeconds, double amplitude, double periodSeconds, double phase)
{
    if (periodSeconds <= 0.0 || amplitude == 0.0) {
        return 0.0;
    }

    return amplitude * std::sin((elapsedSeconds / periodSeconds) * 2.0 * Pi + phase);
}

double gaussian(std::mt19937 &random, double mean, double sigma)
{
    if (sigma <= 0.0) {
        return mean;
    }

    std::normal_distribution<double> distribution(mean, sigma);
    return distribution(random);
}

double roundTo(double value, int decimals)
{
    const auto factor = std::pow(10.0, decimals);
    return std::round(value * factor) / factor;
}

} // namespace Monitor::Simulator::Noise
