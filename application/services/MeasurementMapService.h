#ifndef MEASUREMENTMAPSERVICE_H
#define MEASUREMENTMAPSERVICE_H

#include "application/caches/MatrixFrameCache.h"
#include "application/dto/ApplicationDtos.h"
#include "application/services/MeasurementMapAnalysis.h"

#include <optional>

namespace Monitor::Application::Services {

class MeasurementMapService
{
public:
    MeasurementMapService();

    void update(const Monitor::Domain::Measurements::MatrixFrame &frame);
    std::optional<Monitor::Application::Dtos::MatrixFrameDto> latest() const;
    std::optional<Monitor::Application::Dtos::MatrixAnalysisSnapshot> latestAnalysis() const;
    std::optional<Monitor::Application::Dtos::MeasurementMapSnapshot> latestSnapshot(
        const Monitor::Application::Dtos::MatrixDisplayOptions &options = {}) const;

    Monitor::Application::Dtos::MatrixAnalysisSnapshot analyze(
        const Monitor::Domain::Measurements::MatrixFrame &frame) const;
    Monitor::Application::Dtos::MeasurementMapSnapshot buildMeasurementMapSnapshot(
        const Monitor::Application::Dtos::MatrixAnalysisSnapshot &analysis,
        const Monitor::Application::Dtos::MatrixDisplayOptions &options = {}) const;
    Monitor::Application::Dtos::MatrixPreview buildMatrixPreview(
        const Monitor::Application::Dtos::MatrixAnalysisSnapshot &analysis) const;

private:
    static Monitor::Application::Dtos::MatrixFrameDto mapFrame(
        const Monitor::Domain::Measurements::MatrixFrame &frame,
        const Monitor::Domain::Measurements::MatrixStatistics &statistics);
    static std::optional<Monitor::Application::Dtos::MatrixPreviewPoint> selectMainAbnormalPoint(
        const Monitor::Application::Dtos::MatrixAnalysisSnapshot &analysis);
    static int severityPriority(Monitor::Application::Dtos::MatrixSeverity severity);
    static int typePriority(Monitor::Application::Dtos::MatrixAbnormalType type);
    static double deviationFromAverage(double value, double average);

    Monitor::Application::Caches::MatrixFrameCache m_cache;
    Monitor::Application::Services::MeasurementMap::AbnormalPointDetector m_abnormalPointDetector;
    Monitor::Application::Services::MeasurementMap::MatrixScaleService m_scaleService;
    Monitor::Application::Services::MeasurementMap::IndustrialHeatColorMapService m_colorMapService;
    Monitor::Application::Services::MeasurementMap::MatrixQualityEvaluator m_qualityEvaluator;
    Monitor::Application::Services::MeasurementMap::MatrixAbnormalDetectionOptions m_detectionOptions;
};

} // namespace Monitor::Application::Services

#endif // MEASUREMENTMAPSERVICE_H
