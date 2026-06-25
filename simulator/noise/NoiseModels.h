#ifndef NOISEMODELS_H
#define NOISEMODELS_H

#include <random>

namespace Monitor::Simulator::Noise {

double sineWave(double elapsedSeconds, double amplitude, double periodSeconds, double phase);
double gaussian(std::mt19937 &random, double mean, double sigma);
double roundTo(double value, int decimals);

} // namespace Monitor::Simulator::Noise

#endif // NOISEMODELS_H
