#ifndef MATRIXSTATISTICSCALCULATOR_H
#define MATRIXSTATISTICSCALCULATOR_H

#include "domain/measurements/MeasurementModels.h"

#include <QVector>

namespace Monitor::Domain::Rules {

class MatrixStatisticsCalculator
{
public:
    static Measurements::MatrixStatistics calculate(const QVector<double> &values);
};

class MatrixStatisticsRule
{
public:
    static Measurements::MatrixStatistics calculate(const Measurements::MatrixFrame &frame);
};

} // namespace Monitor::Domain::Rules

#endif // MATRIXSTATISTICSCALCULATOR_H
