#ifndef MEASUREMENTMAPANALYSIS_H
#define MEASUREMENTMAPANALYSIS_H

#include "application/dto/ApplicationDtos.h"
#include "domain/measurements/MeasurementModels.h"

#include <optional>

namespace Monitor::Application::Services::MeasurementMap {

struct MatrixAbnormalDetectionOptions
{
    std::optional<double> highLimit = 1500.0;
    std::optional<double> lowLimit = 100.0;
    double zScoreThreshold = 2.5;
    double localStdDevMultiplier = 1.8;
    double localRelativeThreshold = 0.12;

    static MatrixAbnormalDetectionOptions defaults();
};

class AbnormalPointDetector
{
public:
    QVector<Monitor::Application::Dtos::AbnormalMatrixPoint> detect(
        const Monitor::Domain::Measurements::MatrixFrame &frame,
        const Monitor::Domain::Measurements::MatrixStatistics &statistics,
        const MatrixAbnormalDetectionOptions &options = MatrixAbnormalDetectionOptions::defaults()) const;

private:
    static bool tryGetLocalMedian(
        const Monitor::Domain::Measurements::MatrixFrame &frame,
        int row,
        int column,
        double *median);
    static void validateOptions(const MatrixAbnormalDetectionOptions &options);
};

class MatrixQualityEvaluator
{
public:
    Monitor::Application::Dtos::MatrixQualityState evaluate(
        const Monitor::Application::Dtos::MatrixStatisticsDto &statistics,
        const QVector<Monitor::Application::Dtos::AbnormalMatrixPoint> &abnormalPoints) const;
};

class MatrixScaleService
{
public:
    Monitor::Application::Dtos::ScaleRange resolve(
        const Monitor::Application::Dtos::MatrixStatisticsDto &statistics,
        const Monitor::Application::Dtos::MatrixDisplayOptions &options) const;
    double normalize(double value, const Monitor::Application::Dtos::ScaleRange &scaleRange) const;
};

class IndustrialHeatColorMapService
{
public:
    Monitor::Application::Dtos::RgbColor colorFor(double normalizedValue) const;
};

} // namespace Monitor::Application::Services::MeasurementMap

#endif // MEASUREMENTMAPANALYSIS_H
