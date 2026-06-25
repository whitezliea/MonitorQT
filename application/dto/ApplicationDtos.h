#ifndef APPLICATIONDTOS_H
#define APPLICATIONDTOS_H

#include "domain/alarms/AlarmModels.h"
#include "domain/logs/LogModels.h"
#include "domain/measurements/MeasurementModels.h"
#include "domain/tags/TagModels.h"
#include "application/configuration/MonitorRuntimeOptions.h"
#include "application/configuration/TagRuntimeConfiguration.h"

#include <QDateTime>
#include <QString>
#include <QUuid>
#include <QVector>

#include <optional>

namespace Monitor::Application::Dtos {

enum class AlarmLifecycleChangeType {
    Raised,
    Updated,
    Recovered
};

struct AlarmLifecycleChange
{
    AlarmLifecycleChangeType changeType = AlarmLifecycleChangeType::Raised;
    Monitor::Domain::Alarms::AlarmEvent alarm;
};

struct AlarmEvaluationResult
{
    QVector<Monitor::Domain::Tags::TagRuntimeState> states;
    QVector<AlarmLifecycleChange> lifecycleChanges;
};

struct DashboardSnapshot
{
    QDateTime timestampUtc;
    QVector<Monitor::Domain::Tags::TagRuntimeState> tags;
    QVector<Monitor::Domain::Alarms::AlarmEvent> activeAlarms;
    int totalTagCount = 0;
    int badQualityCount = 0;
    QUuid sourceFrameId;
    qint64 sequenceNo = 0;
};

struct TrendPointDto
{
    QDateTime timestampUtc;
    double value = 0.0;
    Monitor::Domain::Tags::TagQuality quality = Monitor::Domain::Tags::TagQuality::Good;
    bool isSpike = false;
};

struct TrendSeries
{
    QString tagId;
    QVector<TrendPointDto> points;
    int requestedPointCount = 0;
    QUuid sourceFrameId;
    qint64 sequenceNo = 0;
    std::optional<QDateTime> sourceTimestampUtc;

    bool isWindowComplete() const
    {
        return requestedPointCount > 0 && points.size() >= requestedPointCount;
    }
};

enum class MatrixAbnormalType {
    InvalidValue,
    HighLimit,
    LowLimit,
    StatisticalHotspot,
    StatisticalColdspot,
    LocalHotspot,
    LocalColdspot
};

enum class MatrixSeverity {
    Info,
    Warning,
    Alarm
};

enum class MatrixQualityState {
    Good,
    Attention,
    Warning,
    Alarm
};

enum class MatrixScaleMode {
    AutoCurrentFrame,
    FixedEngineeringRange
};

enum class MatrixPalette {
    IndustrialHeat
};

struct AbnormalMatrixPoint
{
    int row = 0;
    int column = 0;
    double value = 0.0;
    MatrixAbnormalType type = MatrixAbnormalType::InvalidValue;
    MatrixSeverity severity = MatrixSeverity::Info;
    QString message;
};

struct MatrixStatisticsDto
{
    double minValue = 0.0;
    double maxValue = 0.0;
    double averageValue = 0.0;
    double stdDev = 0.0;
    double uniformityMinMax = 0.0;
    double uniformityMinAverage = 0.0;
    int validCount = 0;
    int invalidCount = 0;

    static MatrixStatisticsDto fromDomain(const Monitor::Domain::Measurements::MatrixStatistics &statistics)
    {
        return {
            statistics.minValue,
            statistics.maxValue,
            statistics.averageValue,
            statistics.stdDev,
            statistics.uniformityMinMax,
            statistics.uniformityMinAverage,
            statistics.validCount,
            statistics.invalidCount
        };
    }

    double uniformity() const
    {
        return uniformityMinMax;
    }
};

struct MatrixFrameDto
{
    QDateTime timestampUtc;
    int rows = 0;
    int columns = 0;
    QVector<double> values;
    MatrixStatisticsDto statistics;
    QUuid sourceFrameId;
    qint64 sequenceNo = 0;

    double valueAt(int row, int column) const
    {
        return values.at(row * columns + column);
    }
};

struct MatrixAnalysisSnapshot
{
    QDateTime timestampUtc;
    MatrixFrameDto frame;
    MatrixStatisticsDto statistics;
    QVector<AbnormalMatrixPoint> abnormalPoints;
    MatrixQualityState qualityState = MatrixQualityState::Good;
    QUuid sourceFrameId;
    qint64 sequenceNo = 0;
};

struct RgbColor
{
    int r = 0;
    int g = 0;
    int b = 0;
};

struct ScaleRange
{
    double minValue = 0.0;
    double maxValue = 0.0;

    double range() const
    {
        return maxValue - minValue;
    }
};

struct MatrixDisplayOptions
{
    MatrixScaleMode scaleMode = MatrixScaleMode::AutoCurrentFrame;
    std::optional<double> fixedMin;
    std::optional<double> fixedMax;
    MatrixPalette palette = MatrixPalette::IndustrialHeat;
    QString matrixType = QStringLiteral("Light Intensity");
    QString unit = QStringLiteral("lux");
};

struct HeatmapCell
{
    int row = 0;
    int column = 0;
    double value = 0.0;
    double normalizedValue = 0.0;
    RgbColor color;
    bool isValid = true;
    bool isAbnormal = false;
    std::optional<MatrixAbnormalType> abnormalType;
    std::optional<MatrixSeverity> severity;
    QString displayText;
    QString tooltipText;
};

struct MeasurementMapSnapshot
{
    QDateTime timestampUtc;
    QString matrixType;
    QString unit;
    MatrixFrameDto frame;
    ScaleRange scaleRange;
    MatrixStatisticsDto statistics;
    QVector<HeatmapCell> cells;
    QVector<AbnormalMatrixPoint> abnormalPoints;
    MatrixQualityState qualityState = MatrixQualityState::Good;
    QUuid sourceFrameId;
    qint64 sequenceNo = 0;
};

struct MatrixPreviewPoint
{
    int row = 0;
    int column = 0;
    double value = 0.0;
    MatrixAbnormalType type = MatrixAbnormalType::InvalidValue;
    MatrixSeverity severity = MatrixSeverity::Info;
};

struct MatrixPreviewCell
{
    int row = 0;
    int column = 0;
    double normalizedValue = 0.0;
    RgbColor color;
    bool isValid = true;
    bool isAbnormal = false;
    MatrixSeverity severity = MatrixSeverity::Info;
};

struct MatrixPreview
{
    QDateTime timestampUtc;
    int rows = 0;
    int columns = 0;
    QString matrixType;
    QString unit;
    MatrixQualityState qualityState = MatrixQualityState::Good;
    double maxValue = 0.0;
    double averageValue = 0.0;
    double uniformityMinMax = 0.0;
    int abnormalCount = 0;
    std::optional<MatrixPreviewPoint> mainAbnormalPoint;
    QVector<MatrixPreviewCell> cells;
    QUuid sourceFrameId;
    qint64 sequenceNo = 0;
};

struct ShellSnapshot
{
    bool running = false;
    bool dataSourceConnected = false;
    bool databaseConnected = false;
    quint64 lastFrameIndex = 0;
    qint64 matrixFrameIndex = 0;
    QString syncState = QStringLiteral("Idle");
    QDateTime capturedAtUtc;
};

struct UiSnapshot
{
    ShellSnapshot shell;
    DashboardSnapshot dashboard;
    Monitor::Domain::Tags::TagSnapshot tags;
    QVector<Monitor::Domain::Alarms::AlarmEvent> currentAlarms;
    QVector<Monitor::Domain::Alarms::AlarmEvent> alarmHistory;
    QVector<Monitor::Domain::Tags::TagValue> historySamples;
    QVector<Monitor::Domain::Logs::OperationLog> operationLogs;
    std::optional<MeasurementMapSnapshot> measurementMap;
    Monitor::Application::Configuration::MonitorRuntimeOptions runtimeOptions;
    QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> tagConfigurations;
    QVector<Monitor::Domain::Tags::TagDefinition> tagDefinitions;
};

} // namespace Monitor::Application::Dtos

#endif // APPLICATIONDTOS_H
