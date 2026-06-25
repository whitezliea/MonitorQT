#include "TrendDiagnosisOptions.h"

#include <cmath>
#include <stdexcept>

namespace Monitor::Application::Configuration {
namespace {

void throwArgument(const char *message)
{
    throw std::invalid_argument(message);
}

} // namespace

void TrendDiagnosisOptions::validate() const
{
    if (spikeLookback < 3) {
        throwArgument("Spike lookback must be at least 3 points.");
    }

    if (!std::isfinite(spikeMadMultiplier) || spikeMadMultiplier <= 0) {
        throwArgument("Spike MAD multiplier must be a positive finite number.");
    }

    if (!std::isfinite(minimumSpikePercentOfSpan) || minimumSpikePercentOfSpan <= 0) {
        throwArgument("Minimum spike percent of span must be a positive finite number.");
    }

    if (driftMinimumPoints < 2) {
        throwArgument("Drift minimum points must be at least 2.");
    }

    if (driftWindowMs <= 0) {
        throwArgument("Drift window must be greater than zero.");
    }

    if (!std::isfinite(driftThresholdPercentOfSpanPerMinute) ||
        driftThresholdPercentOfSpanPerMinute <= 0) {
        throwArgument("Drift threshold must be a positive finite number.");
    }
}

} // namespace Monitor::Application::Configuration
