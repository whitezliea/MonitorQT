#include "MeasurementMapService.h"

#include "domain/common/DomainCommon.h"

#include <QHash>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace Monitor::Application::Services {
namespace {

const Monitor::Application::Dtos::MatrixDisplayOptions &previewDisplayOptions()
{
    static const Monitor::Application::Dtos::MatrixDisplayOptions options{
        Monitor::Application::Dtos::MatrixScaleMode::AutoCurrentFrame,
        std::nullopt,
        std::nullopt,
        Monitor::Application::Dtos::MatrixPalette::IndustrialHeat,
        QStringLiteral("Light Intensity"),
        QStringLiteral("lux")
    };
    return options;
}

QString abnormalTypeText(Monitor::Application::Dtos::MatrixAbnormalType type)
{
    using Monitor::Application::Dtos::MatrixAbnormalType;

    switch (type) {
    case MatrixAbnormalType::InvalidValue:
        return QStringLiteral("InvalidValue");
    case MatrixAbnormalType::HighLimit:
        return QStringLiteral("HighLimit");
    case MatrixAbnormalType::LowLimit:
        return QStringLiteral("LowLimit");
    case MatrixAbnormalType::StatisticalHotspot:
        return QStringLiteral("StatisticalHotspot");
    case MatrixAbnormalType::StatisticalColdspot:
        return QStringLiteral("StatisticalColdspot");
    case MatrixAbnormalType::LocalHotspot:
        return QStringLiteral("LocalHotspot");
    case MatrixAbnormalType::LocalColdspot:
        return QStringLiteral("LocalColdspot");
    }

    return {};
}

} // namespace

MeasurementMapService::MeasurementMapService()
    : m_detectionOptions(MeasurementMap::MatrixAbnormalDetectionOptions::defaults())
{
}

void MeasurementMapService::update(const Monitor::Domain::Measurements::MatrixFrame &frame)
{
    m_cache.update(frame);
}

std::optional<Monitor::Application::Dtos::MatrixFrameDto> MeasurementMapService::latest() const
{
    const auto frame = m_cache.latest();
    if (!frame.has_value()) {
        return std::nullopt;
    }

    return mapFrame(frame.value(), frame->calculateStatistics());
}

std::optional<Monitor::Application::Dtos::MatrixAnalysisSnapshot> MeasurementMapService::latestAnalysis() const
{
    const auto frame = m_cache.latest();
    if (!frame.has_value()) {
        return std::nullopt;
    }

    return analyze(frame.value());
}

std::optional<Monitor::Application::Dtos::MeasurementMapSnapshot> MeasurementMapService::latestSnapshot(
    const Monitor::Application::Dtos::MatrixDisplayOptions &options) const
{
    const auto analysis = latestAnalysis();
    if (!analysis.has_value()) {
        return std::nullopt;
    }

    return buildMeasurementMapSnapshot(analysis.value(), options);
}

Monitor::Application::Dtos::MatrixAnalysisSnapshot MeasurementMapService::analyze(
    const Monitor::Domain::Measurements::MatrixFrame &frame) const
{
    Monitor::Domain::Measurements::MeasurementTimeContract::validate(frame);

    const auto statistics = frame.calculateStatistics();
    const auto frameDto = mapFrame(frame, statistics);
    const auto abnormalPoints = m_abnormalPointDetector.detect(frame, statistics, m_detectionOptions);
    const auto qualityState = m_qualityEvaluator.evaluate(frameDto.statistics, abnormalPoints);

    return {
        frame.timestampUtc,
        frameDto,
        frameDto.statistics,
        abnormalPoints,
        qualityState,
        frame.sourceFrameId,
        frame.sequenceNo
    };
}

Monitor::Application::Dtos::MeasurementMapSnapshot MeasurementMapService::buildMeasurementMapSnapshot(
    const Monitor::Application::Dtos::MatrixAnalysisSnapshot &analysis,
    const Monitor::Application::Dtos::MatrixDisplayOptions &options) const
{
    using Monitor::Application::Dtos::HeatmapCell;

    const auto scaleRange = m_scaleService.resolve(analysis.statistics, options);
    QHash<QString, Monitor::Application::Dtos::AbnormalMatrixPoint> abnormalByCoordinate;
    for (const auto &point : analysis.abnormalPoints) {
        abnormalByCoordinate.insert(QStringLiteral("%1:%2").arg(point.row).arg(point.column), point);
    }

    QVector<HeatmapCell> cells;
    cells.reserve(analysis.frame.rows * analysis.frame.columns);
    for (auto row = 0; row < analysis.frame.rows; ++row) {
        for (auto column = 0; column < analysis.frame.columns; ++column) {
            const auto value = analysis.frame.valueAt(row, column);
            const auto isValid = std::isfinite(value);
            const auto normalizedValue = m_scaleService.normalize(value, scaleRange);
            const auto coordinateKey = QStringLiteral("%1:%2").arg(row).arg(column);
            const auto abnormalIt = abnormalByCoordinate.constFind(coordinateKey);
            const auto displayText = isValid
                ? QString::number(value, 'f', 3).remove(QRegularExpression(QStringLiteral("\\.?0+$")))
                : QStringLiteral("NA");
            auto tooltipText = QStringLiteral("Row %1, Col %2\nValue: %3 %4")
                .arg(row)
                .arg(column)
                .arg(displayText, options.unit);
            if (abnormalIt != abnormalByCoordinate.cend()) {
                tooltipText += QStringLiteral("\nAbnormal: %1").arg(abnormalTypeText(abnormalIt.value().type));
            }

            cells.append(HeatmapCell{
                row,
                column,
                value,
                normalizedValue,
                m_colorMapService.colorFor(normalizedValue),
                isValid,
                abnormalIt != abnormalByCoordinate.cend(),
                abnormalIt == abnormalByCoordinate.cend()
                    ? std::optional<Monitor::Application::Dtos::MatrixAbnormalType>()
                    : std::optional<Monitor::Application::Dtos::MatrixAbnormalType>(abnormalIt.value().type),
                abnormalIt == abnormalByCoordinate.cend()
                    ? std::optional<Monitor::Application::Dtos::MatrixSeverity>()
                    : std::optional<Monitor::Application::Dtos::MatrixSeverity>(abnormalIt.value().severity),
                displayText,
                tooltipText
            });
        }
    }

    return {
        analysis.timestampUtc,
        options.matrixType,
        options.unit,
        analysis.frame,
        scaleRange,
        analysis.statistics,
        cells,
        analysis.abnormalPoints,
        analysis.qualityState,
        analysis.sourceFrameId,
        analysis.sequenceNo
    };
}

Monitor::Application::Dtos::MatrixPreview MeasurementMapService::buildMatrixPreview(
    const Monitor::Application::Dtos::MatrixAnalysisSnapshot &analysis) const
{
    using Monitor::Application::Dtos::MatrixPreview;
    using Monitor::Application::Dtos::MatrixPreviewCell;
    using Monitor::Application::Dtos::MatrixSeverity;

    const auto &options = previewDisplayOptions();
    const auto snapshot = buildMeasurementMapSnapshot(analysis, options);
    QVector<MatrixPreviewCell> cells;
    cells.reserve(snapshot.cells.size());
    for (const auto &cell : snapshot.cells) {
        cells.append(MatrixPreviewCell{
            cell.row,
            cell.column,
            cell.normalizedValue,
            cell.color,
            cell.isValid,
            cell.isAbnormal,
            cell.severity.value_or(MatrixSeverity::Info)
        });
    }

    MatrixPreview preview;
    preview.timestampUtc = analysis.timestampUtc;
    preview.rows = analysis.frame.rows;
    preview.columns = analysis.frame.columns;
    preview.matrixType = options.matrixType;
    preview.unit = options.unit;
    preview.qualityState = analysis.qualityState;
    preview.maxValue = analysis.statistics.maxValue;
    preview.averageValue = analysis.statistics.averageValue;
    preview.uniformityMinMax = analysis.statistics.uniformityMinMax;
    preview.abnormalCount = analysis.abnormalPoints.size();
    preview.mainAbnormalPoint = selectMainAbnormalPoint(analysis);
    preview.cells = cells;
    preview.sourceFrameId = analysis.sourceFrameId;
    preview.sequenceNo = analysis.sequenceNo;
    return preview;
}

Monitor::Application::Dtos::MatrixFrameDto MeasurementMapService::mapFrame(
    const Monitor::Domain::Measurements::MatrixFrame &frame,
    const Monitor::Domain::Measurements::MatrixStatistics &statistics)
{
    return {
        frame.timestampUtc,
        frame.rows,
        frame.columns,
        frame.values,
        Monitor::Application::Dtos::MatrixStatisticsDto::fromDomain(statistics),
        frame.sourceFrameId,
        frame.sequenceNo
    };
}

std::optional<Monitor::Application::Dtos::MatrixPreviewPoint> MeasurementMapService::selectMainAbnormalPoint(
    const Monitor::Application::Dtos::MatrixAnalysisSnapshot &analysis)
{
    if (analysis.abnormalPoints.isEmpty()) {
        return std::nullopt;
    }

    auto points = analysis.abnormalPoints;
    std::sort(points.begin(), points.end(), [&analysis](const auto &left, const auto &right) {
        const auto leftSeverity = severityPriority(left.severity);
        const auto rightSeverity = severityPriority(right.severity);
        if (leftSeverity != rightSeverity) {
            return leftSeverity > rightSeverity;
        }

        const auto leftType = typePriority(left.type);
        const auto rightType = typePriority(right.type);
        if (leftType != rightType) {
            return leftType > rightType;
        }

        const auto leftDeviation = deviationFromAverage(left.value, analysis.statistics.averageValue);
        const auto rightDeviation = deviationFromAverage(right.value, analysis.statistics.averageValue);
        if (leftDeviation != rightDeviation) {
            return leftDeviation > rightDeviation;
        }

        if (left.row != right.row) {
            return left.row < right.row;
        }

        return left.column < right.column;
    });

    const auto &point = points.first();
    return Monitor::Application::Dtos::MatrixPreviewPoint{
        point.row,
        point.column,
        point.value,
        point.type,
        point.severity
    };
}

int MeasurementMapService::severityPriority(Monitor::Application::Dtos::MatrixSeverity severity)
{
    using Monitor::Application::Dtos::MatrixSeverity;

    switch (severity) {
    case MatrixSeverity::Alarm:
        return 3;
    case MatrixSeverity::Warning:
        return 2;
    case MatrixSeverity::Info:
        return 1;
    }

    return 0;
}

int MeasurementMapService::typePriority(Monitor::Application::Dtos::MatrixAbnormalType type)
{
    using Monitor::Application::Dtos::MatrixAbnormalType;

    switch (type) {
    case MatrixAbnormalType::InvalidValue:
        return 4;
    case MatrixAbnormalType::HighLimit:
    case MatrixAbnormalType::LowLimit:
        return 3;
    case MatrixAbnormalType::StatisticalHotspot:
    case MatrixAbnormalType::StatisticalColdspot:
        return 2;
    case MatrixAbnormalType::LocalHotspot:
    case MatrixAbnormalType::LocalColdspot:
        return 1;
    }

    return 0;
}

double MeasurementMapService::deviationFromAverage(double value, double average)
{
    return std::isfinite(value) && std::isfinite(average)
        ? std::abs(value - average)
        : 0.0;
}

} // namespace Monitor::Application::Services
