#include "MeasurementMapAnalysis.h"

#include "domain/common/DomainCommon.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace Monitor::Application::Services::MeasurementMap {
namespace {

constexpr double Epsilon = 1e-9;

bool severityMatches(
    const Monitor::Application::Dtos::AbnormalMatrixPoint &point,
    Monitor::Application::Dtos::MatrixSeverity severity)
{
    return point.severity == severity;
}

} // namespace

MatrixAbnormalDetectionOptions MatrixAbnormalDetectionOptions::defaults()
{
    return {};
}

QVector<Monitor::Application::Dtos::AbnormalMatrixPoint> AbnormalPointDetector::detect(
    const Monitor::Domain::Measurements::MatrixFrame &frame,
    const Monitor::Domain::Measurements::MatrixStatistics &statistics,
    const MatrixAbnormalDetectionOptions &options) const
{
    using Monitor::Application::Dtos::AbnormalMatrixPoint;
    using Monitor::Application::Dtos::MatrixAbnormalType;
    using Monitor::Application::Dtos::MatrixSeverity;

    if (!frame.dimensionsMatchValues()) {
        throw Monitor::Domain::Common::DomainException(
            QStringLiteral("Matrix frame dimensions do not match the values array."));
    }

    validateOptions(options);

    QVector<AbnormalMatrixPoint> result;
    result.reserve(frame.rows * frame.columns / 8);

    for (auto row = 0; row < frame.rows; ++row) {
        for (auto column = 0; column < frame.columns; ++column) {
            const auto value = frame.valueAt(row, column);

            if (!std::isfinite(value)) {
                result.append(AbnormalMatrixPoint{row, column, value, MatrixAbnormalType::InvalidValue, MatrixSeverity::Alarm,
                                                  QStringLiteral("Invalid sensor value.")});
                continue;
            }

            if (options.highLimit.has_value() && value > options.highLimit.value()) {
                result.append(AbnormalMatrixPoint{row, column, value, MatrixAbnormalType::HighLimit, MatrixSeverity::Alarm,
                                                  QStringLiteral("Value exceeds the high engineering limit.")});
                continue;
            }

            if (options.lowLimit.has_value() && value < options.lowLimit.value()) {
                result.append(AbnormalMatrixPoint{row, column, value, MatrixAbnormalType::LowLimit, MatrixSeverity::Alarm,
                                                  QStringLiteral("Value is below the low engineering limit.")});
                continue;
            }

            if (std::isfinite(statistics.stdDev) && statistics.stdDev > Epsilon) {
                const auto zScore = (value - statistics.averageValue) / statistics.stdDev;
                if (zScore >= options.zScoreThreshold) {
                    result.append(AbnormalMatrixPoint{row, column, value, MatrixAbnormalType::StatisticalHotspot, MatrixSeverity::Warning,
                                                      QStringLiteral("Statistical hotspot detected.")});
                    continue;
                }

                if (zScore <= -options.zScoreThreshold) {
                    result.append(AbnormalMatrixPoint{row, column, value, MatrixAbnormalType::StatisticalColdspot, MatrixSeverity::Warning,
                                                      QStringLiteral("Statistical coldspot detected.")});
                    continue;
                }
            }

            double localMedian = std::numeric_limits<double>::quiet_NaN();
            if (!tryGetLocalMedian(frame, row, column, &localMedian)) {
                continue;
            }

            const auto localDelta = value - localMedian;
            const auto localRelativeDelta = std::abs(localMedian) < Epsilon
                ? 0.0
                : localDelta / std::abs(localMedian);
            const auto localHotspotByStdDev = statistics.stdDev > Epsilon
                && localDelta > options.localStdDevMultiplier * statistics.stdDev;
            const auto localColdspotByStdDev = statistics.stdDev > Epsilon
                && localDelta < -options.localStdDevMultiplier * statistics.stdDev;

            if (localHotspotByStdDev || localRelativeDelta > options.localRelativeThreshold) {
                result.append(AbnormalMatrixPoint{row, column, value, MatrixAbnormalType::LocalHotspot, MatrixSeverity::Warning,
                                                  QStringLiteral("Local value is significantly higher than neighboring points.")});
                continue;
            }

            if (localColdspotByStdDev || localRelativeDelta < -options.localRelativeThreshold) {
                result.append(AbnormalMatrixPoint{row, column, value, MatrixAbnormalType::LocalColdspot, MatrixSeverity::Warning,
                                                  QStringLiteral("Local value is significantly lower than neighboring points.")});
            }
        }
    }

    return result;
}

bool AbnormalPointDetector::tryGetLocalMedian(
    const Monitor::Domain::Measurements::MatrixFrame &frame,
    int row,
    int column,
    double *median)
{
    QVector<double> neighbors;
    neighbors.reserve(8);

    for (auto currentRow = std::max(0, row - 1); currentRow <= std::min(frame.rows - 1, row + 1); ++currentRow) {
        for (auto currentColumn = std::max(0, column - 1); currentColumn <= std::min(frame.columns - 1, column + 1); ++currentColumn) {
            if (currentRow == row && currentColumn == column) {
                continue;
            }

            const auto value = frame.valueAt(currentRow, currentColumn);
            if (std::isfinite(value)) {
                neighbors.append(value);
            }
        }
    }

    if (neighbors.isEmpty()) {
        if (median) {
            *median = std::numeric_limits<double>::quiet_NaN();
        }
        return false;
    }

    std::sort(neighbors.begin(), neighbors.end());
    const auto middle = neighbors.size() / 2;
    if (median) {
        *median = neighbors.size() % 2 == 1
            ? neighbors.at(middle)
            : (neighbors.at(middle - 1) + neighbors.at(middle)) / 2.0;
    }

    return true;
}

void AbnormalPointDetector::validateOptions(const MatrixAbnormalDetectionOptions &options)
{
    if ((options.highLimit.has_value() && !std::isfinite(options.highLimit.value())) ||
        (options.lowLimit.has_value() && !std::isfinite(options.lowLimit.value()))) {
        throw std::invalid_argument("Engineering limits must be finite when specified.");
    }

    if (options.highLimit.has_value() &&
        options.lowLimit.has_value() &&
        options.lowLimit.value() > options.highLimit.value()) {
        throw std::invalid_argument("LowLimit cannot be greater than HighLimit.");
    }

    if (!std::isfinite(options.zScoreThreshold) ||
        !std::isfinite(options.localStdDevMultiplier) ||
        !std::isfinite(options.localRelativeThreshold) ||
        options.zScoreThreshold <= 0.0 ||
        options.localStdDevMultiplier <= 0.0 ||
        options.localRelativeThreshold <= 0.0) {
        throw std::out_of_range("Detection thresholds must be greater than zero.");
    }
}

Monitor::Application::Dtos::MatrixQualityState MatrixQualityEvaluator::evaluate(
    const Monitor::Application::Dtos::MatrixStatisticsDto &statistics,
    const QVector<Monitor::Application::Dtos::AbnormalMatrixPoint> &abnormalPoints) const
{
    using Monitor::Application::Dtos::MatrixQualityState;
    using Monitor::Application::Dtos::MatrixSeverity;

    if (statistics.invalidCount > 0 ||
        std::any_of(abnormalPoints.cbegin(), abnormalPoints.cend(), [](const auto &point) {
            return severityMatches(point, MatrixSeverity::Alarm);
        })) {
        return MatrixQualityState::Alarm;
    }

    if (std::any_of(abnormalPoints.cbegin(), abnormalPoints.cend(), [](const auto &point) {
            return severityMatches(point, MatrixSeverity::Warning);
        })) {
        return MatrixQualityState::Warning;
    }

    if (std::isfinite(statistics.uniformityMinMax)) {
        if (statistics.uniformityMinMax < 0.70) {
            return MatrixQualityState::Warning;
        }

        if (statistics.uniformityMinMax < 0.80) {
            return MatrixQualityState::Attention;
        }
    }

    return MatrixQualityState::Good;
}

Monitor::Application::Dtos::ScaleRange MatrixScaleService::resolve(
    const Monitor::Application::Dtos::MatrixStatisticsDto &statistics,
    const Monitor::Application::Dtos::MatrixDisplayOptions &options) const
{
    using Monitor::Application::Dtos::MatrixScaleMode;
    using Monitor::Application::Dtos::ScaleRange;

    if (options.scaleMode != MatrixScaleMode::FixedEngineeringRange) {
        return ScaleRange{statistics.minValue, statistics.maxValue};
    }

    if (!options.fixedMin.has_value() || !options.fixedMax.has_value()) {
        throw std::invalid_argument("Fixed engineering range requires fixedMin and fixedMax.");
    }

    if (!std::isfinite(options.fixedMin.value()) || !std::isfinite(options.fixedMax.value())) {
        throw std::invalid_argument("Fixed engineering range values must be finite.");
    }

    if (options.fixedMin.value() > options.fixedMax.value()) {
        throw std::invalid_argument("fixedMin cannot be greater than fixedMax.");
    }

    return ScaleRange{options.fixedMin.value(), options.fixedMax.value()};
}

double MatrixScaleService::normalize(
    double value,
    const Monitor::Application::Dtos::ScaleRange &scaleRange) const
{
    if (!std::isfinite(value) ||
        !std::isfinite(scaleRange.minValue) ||
        !std::isfinite(scaleRange.maxValue)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    if (std::abs(scaleRange.range()) < Epsilon) {
        return 0.5;
    }

    return std::clamp((value - scaleRange.minValue) / scaleRange.range(), 0.0, 1.0);
}

Monitor::Application::Dtos::RgbColor IndustrialHeatColorMapService::colorFor(double normalizedValue) const
{
    using Monitor::Application::Dtos::RgbColor;

    if (!std::isfinite(normalizedValue)) {
        return RgbColor{96, 96, 96};
    }

    const auto clamped = std::clamp(normalizedValue, 0.0, 1.0);
    if (clamped < 0.25) {
        const auto t = clamped / 0.25;
        return RgbColor{
            static_cast<int>(18 + 28 * t),
            static_cast<int>(38 + 48 * t),
            static_cast<int>(79 + 54 * t)
        };
    }

    if (clamped < 0.50) {
        const auto t = (clamped - 0.25) / 0.25;
        return RgbColor{
            static_cast<int>(46 + 14 * t),
            static_cast<int>(86 + 118 * t),
            static_cast<int>(133 - 42 * t)
        };
    }

    if (clamped < 0.75) {
        const auto t = (clamped - 0.50) / 0.25;
        return RgbColor{
            static_cast<int>(60 + 185 * t),
            static_cast<int>(204 - 76 * t),
            static_cast<int>(91 - 48 * t)
        };
    }

    const auto t = (clamped - 0.75) / 0.25;
    return RgbColor{
        static_cast<int>(245 + 10 * t),
        static_cast<int>(128 + 82 * t),
        static_cast<int>(43 + 99 * t)
    };
}

} // namespace Monitor::Application::Services::MeasurementMap
