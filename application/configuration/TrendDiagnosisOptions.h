#ifndef TRENDDIAGNOSISOPTIONS_H
#define TRENDDIAGNOSISOPTIONS_H

namespace Monitor::Application::Configuration {

struct TrendDiagnosisOptions
{
    int spikeLookback = 15;
    double spikeMadMultiplier = 6.0;
    double minimumSpikePercentOfSpan = 1.0;
    int driftMinimumPoints = 20;
    int driftWindowMs = 60'000;
    double driftThresholdPercentOfSpanPerMinute = 1.0;

    void validate() const;
};

} // namespace Monitor::Application::Configuration

#endif // TRENDDIAGNOSISOPTIONS_H
