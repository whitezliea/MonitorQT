#include "SourceBehaviorFreeze.h"

#include <QHash>
#include <QPair>
#include <QSet>
#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>

namespace Phase0 {
namespace {

constexpr double Epsilon = 1e-9;
constexpr double MatrixHighLimit = 1500.0;
constexpr double MatrixLowLimit = 100.0;
constexpr double MatrixZScoreThreshold = 2.5;
constexpr double MatrixLocalStdDevMultiplier = 1.8;
constexpr double MatrixLocalRelativeThreshold = 0.12;

QString text(const char *value)
{
    return QString::fromUtf8(value);
}

std::optional<double> opt(double value)
{
    return std::optional<double>(value);
}

TagDefinition makeDefinition(
    const char *tagId,
    const char *displayName,
    TagCategory category,
    const char *unit,
    std::optional<double> minValue = std::nullopt,
    std::optional<double> maxValue = std::nullopt,
    std::optional<double> warningHigh = std::nullopt,
    std::optional<double> alarmHigh = std::nullopt,
    std::optional<double> warningLow = std::nullopt,
    std::optional<double> alarmLow = std::nullopt,
    bool isEnabled = true,
    const char *description = "",
    TagDataType dataType = TagDataType::Double,
    TagValueKind valueKind = TagValueKind::Numeric,
    bool isHistorized = true,
    std::optional<int> historyIntervalMs = 1000,
    int displayOrder = 0)
{
    TagDefinition definition;
    definition.tagId = text(tagId);
    definition.displayName = text(displayName);
    definition.category = category;
    definition.unit = text(unit);
    definition.minValue = minValue;
    definition.maxValue = maxValue;
    definition.warningHigh = warningHigh;
    definition.alarmHigh = alarmHigh;
    definition.warningLow = warningLow;
    definition.alarmLow = alarmLow;
    definition.isEnabled = isEnabled;
    definition.description = text(description);
    definition.dataType = dataType;
    definition.valueKind = valueKind;
    definition.isHistorized = isHistorized;
    definition.historyIntervalMs = historyIntervalMs;
    definition.displayOrder = displayOrder;
    return definition;
}

TagSourceMapping makeMapping(
    const char *tagId,
    SourceType sourceType,
    const char *sourceCode = "",
    const char *sourcePath = "",
    const char *formula = "",
    const char *inputTagIds = "")
{
    TagSourceMapping mapping;
    mapping.tagId = text(tagId);
    mapping.sourceDeviceId = defaultDeviceId();
    mapping.sourceType = sourceType;
    mapping.sourceCode = text(sourceCode);
    mapping.sourcePath = text(sourcePath);
    mapping.formula = text(formula);
    mapping.inputTagIds = text(inputTagIds);
    return mapping;
}

const TagDefinition *findDefinition(const QVector<TagDefinition> &definitions, const QString &tagId)
{
    for (const auto &definition : definitions) {
        if (definition.tagId == tagId) {
            return &definition;
        }
    }

    return nullptr;
}

const TagSourceMapping *findMapping(const QVector<TagSourceMapping> &mappings, const QString &tagId)
{
    for (const auto &mapping : mappings) {
        if (mapping.tagId == tagId) {
            return &mapping;
        }
    }

    return nullptr;
}

const ChannelValue *findChannel(const RawMeasurementFrame &frame, const QString &channelId)
{
    for (const auto &channel : frame.channelValues) {
        if (channel.channelId == channelId) {
            return &channel;
        }
    }

    return nullptr;
}

bool nearlyEqual(double left, double right, double epsilon = 1e-6)
{
    return std::abs(left - right) <= epsilon;
}

TagQuality applyRangeQuality(double value, TagQuality quality, const TagDefinition &definition)
{
    if (quality != TagQuality::Good) {
        return quality;
    }

    if (definition.minValue.has_value() && value < definition.minValue.value()) {
        return TagQuality::OutOfRange;
    }

    if (definition.maxValue.has_value() && value > definition.maxValue.value()) {
        return TagQuality::OutOfRange;
    }

    return quality;
}

TagQuality evaluateChannelQuality(
    const RawMeasurementFrame &frame,
    const ChannelValue &channel,
    const TagDefinition &definition,
    const std::optional<double> &numericValue)
{
    if (frame.deviceStatus == DeviceStatus::Offline) {
        return TagQuality::Offline;
    }

    if (channel.quality != TagQuality::Good) {
        return channel.quality;
    }

    if (!numericValue.has_value()) {
        return TagQuality::Bad;
    }

    if (frame.errorCode != 0 || frame.quality == TagQuality::DeviceError) {
        return TagQuality::DeviceError;
    }

    return applyRangeQuality(numericValue.value(), frame.quality, definition);
}

TagAlarmState evaluateAlarmState(double value, TagQuality quality, const TagDefinition *definition)
{
    if (quality == TagQuality::Offline) {
        return TagAlarmState::Offline;
    }

    if (quality != TagQuality::Good) {
        return TagAlarmState::Invalid;
    }

    if (!definition) {
        return TagAlarmState::Normal;
    }

    if (definition->alarmHigh.has_value() && value >= definition->alarmHigh.value()) {
        return TagAlarmState::AlarmHigh;
    }

    if (definition->alarmLow.has_value() && value <= definition->alarmLow.value()) {
        return TagAlarmState::AlarmLow;
    }

    if (definition->warningHigh.has_value() && value >= definition->warningHigh.value()) {
        return TagAlarmState::WarningHigh;
    }

    if (definition->warningLow.has_value() && value <= definition->warningLow.value()) {
        return TagAlarmState::WarningLow;
    }

    return TagAlarmState::Normal;
}

double numericValueForAlarm(const CleanedTagValue &value)
{
    if (value.numericValue.has_value()) {
        return value.numericValue.value();
    }

    if (value.boolValue.has_value()) {
        return value.boolValue.value() ? 1.0 : 0.0;
    }

    return 0.0;
}

CleanedTagValue createCleanedValue(
    const RawMeasurementFrame &frame,
    const TagDefinition &definition,
    const TagSourceMapping &mapping,
    std::optional<double> numericValue,
    std::optional<QString> textValue,
    std::optional<bool> boolValue,
    TagQuality quality,
    const QString &cleanMessage = QString())
{
    CleanedTagValue value;
    value.tagId = definition.tagId;
    value.numericValue = numericValue;
    value.textValue = textValue;
    value.boolValue = boolValue;
    value.dataType = definition.dataType;
    value.unit = definition.unit;
    value.timestampUtc = frame.timestampUtc;
    value.quality = quality;
    value.alarmState = evaluateAlarmState(numericValue.value_or(boolValue.has_value() && boolValue.value() ? 1.0 : 0.0),
                                          quality,
                                          &definition);
    value.sourceDeviceId = frame.deviceId;
    value.sourceCode = mapping.sourceCode;
    value.sourceFrameId = frame.frameId;
    value.sequenceNo = frame.sequenceNo;
    value.cleanMessage = cleanMessage;
    return value;
}

MatrixStatistics calculateMatrixStatistics(const MatrixFrame &matrix)
{
    MatrixStatistics statistics;
    auto minValue = std::numeric_limits<double>::infinity();
    auto maxValue = -std::numeric_limits<double>::infinity();
    auto mean = 0.0;
    auto sumOfSquaredDifferences = 0.0;
    auto validCount = 0;
    auto invalidCount = 0;

    for (const auto value : matrix.values) {
        if (!std::isfinite(value)) {
            ++invalidCount;
            continue;
        }

        minValue = std::min(minValue, value);
        maxValue = std::max(maxValue, value);

        ++validCount;
        const auto delta = value - mean;
        mean += delta / validCount;
        const auto deltaFromUpdatedMean = value - mean;
        sumOfSquaredDifferences += delta * deltaFromUpdatedMean;
    }

    if (validCount == 0) {
        const auto nan = std::numeric_limits<double>::quiet_NaN();
        statistics.minValue = nan;
        statistics.maxValue = nan;
        statistics.averageValue = nan;
        statistics.stdDev = nan;
        statistics.uniformityMinMax = nan;
        statistics.uniformityMinAverage = nan;
        statistics.validCount = 0;
        statistics.invalidCount = invalidCount;
        return statistics;
    }

    const auto variance = sumOfSquaredDifferences / validCount;
    statistics.minValue = minValue;
    statistics.maxValue = maxValue;
    statistics.averageValue = mean;
    statistics.stdDev = std::sqrt(std::max(0.0, variance));
    statistics.uniformityMinMax = std::abs(maxValue) < Epsilon ? std::numeric_limits<double>::quiet_NaN() : minValue / maxValue;
    statistics.uniformityMinAverage = std::abs(mean) < Epsilon ? std::numeric_limits<double>::quiet_NaN() : minValue / mean;
    statistics.validCount = validCount;
    statistics.invalidCount = invalidCount;
    return statistics;
}

bool tryGetLocalMedian(
    const MatrixFrame &matrix,
    int row,
    int column,
    double *median)
{
    QVector<double> neighbors;
    neighbors.reserve(8);

    for (auto currentRow = std::max(0, row - 1); currentRow <= std::min(matrix.rows - 1, row + 1); ++currentRow) {
        for (auto currentColumn = std::max(0, column - 1); currentColumn <= std::min(matrix.columns - 1, column + 1); ++currentColumn) {
            if (currentRow == row && currentColumn == column) {
                continue;
            }

            const auto value = matrix.valueAt(currentRow, currentColumn);
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

int detectAbnormalPointCount(const MatrixFrame &matrix, const MatrixStatistics &statistics)
{
    auto count = 0;

    for (auto row = 0; row < matrix.rows; ++row) {
        for (auto column = 0; column < matrix.columns; ++column) {
            const auto value = matrix.valueAt(row, column);

            if (!std::isfinite(value) || value > MatrixHighLimit || value < MatrixLowLimit) {
                ++count;
                continue;
            }

            if (std::isfinite(statistics.stdDev) && statistics.stdDev > Epsilon) {
                const auto zScore = (value - statistics.averageValue) / statistics.stdDev;
                if (zScore >= MatrixZScoreThreshold || zScore <= -MatrixZScoreThreshold) {
                    ++count;
                    continue;
                }
            }

            double localMedian = 0.0;
            if (!tryGetLocalMedian(matrix, row, column, &localMedian)) {
                continue;
            }

            const auto localDelta = value - localMedian;
            const auto localRelativeDelta = std::abs(localMedian) < Epsilon
                ? 0.0
                : localDelta / std::abs(localMedian);
            const auto localHotspotByStdDev = statistics.stdDev > Epsilon &&
                localDelta > MatrixLocalStdDevMultiplier * statistics.stdDev;
            const auto localColdspotByStdDev = statistics.stdDev > Epsilon &&
                localDelta < -MatrixLocalStdDevMultiplier * statistics.stdDev;

            if (localHotspotByStdDev ||
                localColdspotByStdDev ||
                localRelativeDelta > MatrixLocalRelativeThreshold ||
                localRelativeDelta < -MatrixLocalRelativeThreshold) {
                ++count;
            }
        }
    }

    return count;
}

QPair<int, int> findHotspot(const MatrixFrame &matrix)
{
    auto maxValue = -std::numeric_limits<double>::infinity();
    auto hotspotRow = 0;
    auto hotspotColumn = 0;

    for (auto row = 0; row < matrix.rows; ++row) {
        for (auto column = 0; column < matrix.columns; ++column) {
            const auto value = matrix.valueAt(row, column);
            if (std::isfinite(value) && value > maxValue) {
                maxValue = value;
                hotspotRow = row;
                hotspotColumn = column;
            }
        }
    }

    return {hotspotRow, hotspotColumn};
}

QHash<QString, int> tagIndexes(const QVector<CleanedTagValue> &values)
{
    QHash<QString, int> indexes;
    for (auto index = 0; index < values.size(); ++index) {
        indexes.insert(values.at(index).tagId, index);
    }

    return indexes;
}

void appendFrameFieldTags(
    const RawMeasurementFrame &frame,
    const QVector<TagDefinition> &definitions,
    const QVector<TagSourceMapping> &mappings,
    QVector<CleanedTagValue> *values)
{
    for (const auto &mapping : mappings) {
        if (!mapping.isEnabled ||
            (mapping.sourceType != SourceType::FrameField && mapping.sourceType != SourceType::Runtime)) {
            continue;
        }

        const auto *definition = findDefinition(definitions, mapping.tagId);
        if (!definition) {
            continue;
        }

        const auto quality = frame.deviceStatus == DeviceStatus::Offline ? TagQuality::Offline : frame.quality;
        std::optional<double> numericValue;
        std::optional<QString> textValue;
        std::optional<bool> boolValue;

        if (mapping.sourcePath == QStringLiteral("DeviceStatus")) {
            if (mapping.tagId == QStringLiteral("DEVICE.ONLINE")) {
                boolValue = frame.deviceStatus != DeviceStatus::Offline;
            } else {
                textValue = toString(frame.deviceStatus);
            }
        } else if (mapping.sourcePath == QStringLiteral("ErrorCode")) {
            numericValue = frame.errorCode;
        } else if (mapping.sourcePath == QStringLiteral("Quality")) {
            textValue = toString(frame.quality);
        } else if (mapping.sourcePath == QStringLiteral("SequenceNo")) {
            numericValue = static_cast<double>(frame.sequenceNo);
        } else if (mapping.sourcePath == QStringLiteral("TimestampDelta")) {
            numericValue = 0.0;
        } else if (mapping.sourcePath == QStringLiteral("SequenceNoDelta")) {
            numericValue = 0.0;
        } else {
            continue;
        }

        values->append(createCleanedValue(frame, *definition, mapping, numericValue, textValue, boolValue, quality));
    }
}

void appendChannelTags(
    const RawMeasurementFrame &frame,
    const QVector<TagDefinition> &definitions,
    const QVector<TagSourceMapping> &mappings,
    QVector<CleanedTagValue> *values)
{
    for (const auto &mapping : mappings) {
        if (!mapping.isEnabled || mapping.sourceType != SourceType::Channel) {
            continue;
        }

        const auto *definition = findDefinition(definitions, mapping.tagId);
        const auto *channel = findChannel(frame, mapping.sourceCode);
        if (!definition || !channel) {
            continue;
        }

        const auto valueIsMissing = std::isnan(channel->value);
        const std::optional<double> numericValue = valueIsMissing
            ? std::optional<double>()
            : std::optional<double>(channel->value * mapping.scale + mapping.offset);
        const auto quality = evaluateChannelQuality(frame, *channel, *definition, numericValue);
        const auto cleanMessage = valueIsMissing ? text("Raw channel value is missing.") : QString();

        values->append(createCleanedValue(frame, *definition, mapping, numericValue, std::nullopt, std::nullopt, quality, cleanMessage));
    }
}

void appendDerivedNumeric(
    const RawMeasurementFrame &frame,
    const QVector<TagDefinition> &definitions,
    const QVector<TagSourceMapping> &mappings,
    const QString &tagId,
    const QVector<QString> &inputTagIds,
    const std::function<double(const QVector<double> &)> &calculate,
    QVector<CleanedTagValue> *values)
{
    const auto *definition = findDefinition(definitions, tagId);
    const auto *mapping = findMapping(mappings, tagId);
    if (!definition || !mapping) {
        return;
    }

    const auto indexes = tagIndexes(*values);
    auto anyOffline = false;
    auto allGood = true;
    auto allValuesPresent = true;
    QVector<double> inputValues;
    inputValues.reserve(inputTagIds.size());

    for (const auto &inputTagId : inputTagIds) {
        if (!indexes.contains(inputTagId)) {
            allGood = false;
            allValuesPresent = false;
            continue;
        }

        const auto &input = values->at(indexes.value(inputTagId));
        anyOffline = anyOffline || input.quality == TagQuality::Offline;
        allGood = allGood && input.quality == TagQuality::Good;

        if (input.numericValue.has_value()) {
            inputValues.append(input.numericValue.value());
        } else {
            allValuesPresent = false;
        }
    }

    auto quality = anyOffline
        ? TagQuality::Offline
        : (allGood && allValuesPresent ? TagQuality::Good : TagQuality::Bad);
    std::optional<double> numericValue;
    if (quality == TagQuality::Good) {
        numericValue = calculate(inputValues);
        quality = applyRangeQuality(numericValue.value(), quality, *definition);
    }

    values->append(createCleanedValue(frame, *definition, *mapping, numericValue, std::nullopt, std::nullopt, quality));
}

void appendDerivedTags(
    const RawMeasurementFrame &frame,
    const QVector<TagDefinition> &definitions,
    const QVector<TagSourceMapping> &mappings,
    QVector<CleanedTagValue> *values)
{
    appendDerivedNumeric(
        frame,
        definitions,
        mappings,
        QStringLiteral("MEAS.POWER.CH01"),
        {QStringLiteral("MEAS.VOLTAGE.CH01"), QStringLiteral("MEAS.CURRENT.CH01")},
        [](const QVector<double> &input) { return input.at(0) * input.at(1); },
        values);

    appendDerivedNumeric(
        frame,
        definitions,
        mappings,
        QStringLiteral("MEAS.LOAD_RATIO.CH01"),
        {QStringLiteral("MEAS.CURRENT.CH01")},
        [](const QVector<double> &input) { return input.at(0) / 5.0 * 100.0; },
        values);
}

void appendMatrixStatisticTags(
    const RawMeasurementFrame &frame,
    const QVector<TagDefinition> &definitions,
    const QVector<TagSourceMapping> &mappings,
    const MatrixStatistics &statistics,
    int abnormalPointCount,
    QVector<CleanedTagValue> *values)
{
    if (!frame.hasMatrixValues) {
        return;
    }

    const auto hotspot = findHotspot(frame.matrixValues);
    QHash<QString, double> stats;
    stats.insert(QStringLiteral("MATRIX.LIGHT.AVG"), statistics.averageValue);
    stats.insert(QStringLiteral("MATRIX.LIGHT.MAX"), statistics.maxValue);
    stats.insert(QStringLiteral("MATRIX.LIGHT.MIN"), statistics.minValue);
    stats.insert(QStringLiteral("MATRIX.LIGHT.UNIFORMITY"), statistics.uniformity());
    stats.insert(QStringLiteral("MATRIX.LIGHT.ABNORMAL_COUNT"), abnormalPointCount);
    stats.insert(QStringLiteral("MATRIX.LIGHT.HOTSPOT_ROW"), hotspot.first);
    stats.insert(QStringLiteral("MATRIX.LIGHT.HOTSPOT_COL"), hotspot.second);

    for (const auto &mapping : mappings) {
        if (!mapping.isEnabled || mapping.sourceType != SourceType::Matrix || !stats.contains(mapping.tagId)) {
            continue;
        }

        const auto *definition = findDefinition(definitions, mapping.tagId);
        if (!definition) {
            continue;
        }

        auto quality = frame.deviceStatus == DeviceStatus::Offline ? TagQuality::Offline : frame.quality;
        quality = applyRangeQuality(stats.value(mapping.tagId), quality, *definition);
        values->append(createCleanedValue(frame, *definition, mapping, stats.value(mapping.tagId), std::nullopt, std::nullopt, quality));
    }
}

QVector<QString> historizedTagIds(
    const QVector<TagDefinition> &definitions,
    const QVector<CleanedTagValue> &values)
{
    const auto indexes = tagIndexes(values);
    QVector<QString> result;

    for (const auto &definition : definitions) {
        if (definition.isEnabled && definition.isHistorized && indexes.contains(definition.tagId)) {
            result.append(definition.tagId);
        }
    }

    return result;
}

void addError(QStringList *errors, const QString &message)
{
    if (errors) {
        errors->append(message);
    }
}

} // namespace

int RuntimeOptions::dataSourceTimeoutMs() const
{
    return dataGenerateIntervalMs * dataSourceTimeoutPeriods;
}

int RuntimeOptions::trendPointCount(int windowMinutes) const
{
    if (dataGenerateIntervalMs <= 0) {
        throw std::invalid_argument("Data generation interval must be greater than zero.");
    }

    if (windowMinutes <= 0) {
        throw std::invalid_argument("Trend window must be greater than zero.");
    }

    return static_cast<int>(std::ceil(windowMinutes * 60'000.0 / dataGenerateIntervalMs));
}

int RuntimeOptions::trendBufferCapacity() const
{
    auto maxWindow = 0;
    for (const auto window : trendWindowMinutes) {
        maxWindow = std::max(maxWindow, window);
    }

    return maxWindow == 0 ? 0 : trendPointCount(maxWindow);
}

double MatrixStatistics::uniformity() const
{
    return uniformityMinMax;
}

double MatrixFrame::valueAt(int row, int column) const
{
    return values.at(row * columns + column);
}

const QString &defaultDeviceId()
{
    static const auto id = QStringLiteral("MCMD-001");
    return id;
}

RuntimeOptions defaultRuntimeOptions()
{
    return RuntimeOptions();
}

QVector<TagDefinition> defaultTagDefinitions()
{
    QVector<TagDefinition> definitions;
    definitions.reserve(22);

    definitions.append(makeDefinition("DEVICE.STATUS", u8"设备运行状态", TagCategory::Device, "", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, true, u8"设备当前运行状态", TagDataType::Enum, TagValueKind::Enum, false, 1000, 10));
    definitions.append(makeDefinition("DEVICE.ONLINE", u8"设备在线状态", TagCategory::Device, "", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, true, u8"由 DeviceStatus != Offline 推导", TagDataType::Boolean, TagValueKind::Boolean, true, 1000, 20));
    definitions.append(makeDefinition("DEVICE.ERROR_CODE", u8"设备错误码", TagCategory::Device, "", opt(0), opt(9999), std::nullopt, std::nullopt, std::nullopt, std::nullopt, true, u8"0 表示无错误", TagDataType::Int, TagValueKind::Numeric, true, 1000, 30));
    definitions.append(makeDefinition("DEVICE.QUALITY", u8"设备帧质量", TagCategory::Device, "", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, true, u8"Raw frame quality 映射", TagDataType::Enum, TagValueKind::Enum, true, 1000, 40));
    definitions.append(makeDefinition("DEVICE.SEQUENCE_NO", u8"最新帧序号", TagCategory::Device, "", opt(0), std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, true, u8"设备帧序号", TagDataType::Int, TagValueKind::Numeric, false, 1000, 50));
    definitions.append(makeDefinition("DEVICE.FRAME_INTERVAL_MS", u8"帧间隔", TagCategory::Runtime, "ms", opt(0), opt(5000), opt(750), opt(1500), std::nullopt, std::nullopt, true, u8"根据连续帧 Timestamp 计算", TagDataType::Double, TagValueKind::Numeric, true, 1000, 60));
    definitions.append(makeDefinition("DEVICE.FRAME_LOSS_COUNT", u8"连续丢帧数", TagCategory::Runtime, "frames", opt(0), std::nullopt, opt(1), opt(3), std::nullopt, std::nullopt, true, u8"根据 SequenceNo 跳号计算", TagDataType::Int, TagValueKind::Numeric, true, 1000, 70));

    definitions.append(makeDefinition("MEAS.TEMP.CH01", u8"温度 CH01", TagCategory::Measurement, u8"℃", opt(-20), opt(120), opt(60), opt(80), opt(5), opt(0), true, u8"温度超限演示主 Tag", TagDataType::Double, TagValueKind::Numeric, true, 1000, 110));
    definitions.append(makeDefinition("MEAS.PRESSURE.CH01", u8"压力 CH01", TagCategory::Measurement, "kPa", opt(80), opt(130), opt(115), opt(125), opt(90), opt(85), true, u8"压力稳定性监控", TagDataType::Double, TagValueKind::Numeric, true, 1000, 120));
    definitions.append(makeDefinition("MEAS.LIGHT.CH01", u8"光强 CH01", TagCategory::Measurement, "lux", opt(0), opt(2000), opt(1500), opt(1800), opt(100), opt(50), true, u8"光强单点监控", TagDataType::Double, TagValueKind::Numeric, true, 1000, 130));
    definitions.append(makeDefinition("MEAS.VOLTAGE.CH01", u8"电压 CH01", TagCategory::Electrical, "V", opt(0), opt(30), opt(14), opt(15), opt(10.5), opt(9.5), true, u8"电压跌落演示 Tag", TagDataType::Double, TagValueKind::Numeric, true, 1000, 140));
    definitions.append(makeDefinition("MEAS.CURRENT.CH01", u8"电流 CH01", TagCategory::Electrical, "A", opt(0), opt(5), opt(3), opt(4), opt(0.2), opt(0.1), true, u8"负载电流监控", TagDataType::Double, TagValueKind::Numeric, true, 1000, 150));
    definitions.append(makeDefinition("MEAS.VIBRATION.CH01", u8"振动 CH01", TagCategory::Mechanical, "mm/s", opt(0), opt(10), opt(1.0), opt(2.5), std::nullopt, std::nullopt, true, u8"振动尖峰演示 Tag", TagDataType::Double, TagValueKind::Numeric, true, 1000, 160));

    definitions.append(makeDefinition("MEAS.POWER.CH01", u8"功率 CH01", TagCategory::Derived, "W", opt(0), opt(150), opt(36), opt(48), std::nullopt, std::nullopt, true, u8"电压与电流派生功率", TagDataType::Double, TagValueKind::DerivedNumeric, true, 1000, 210));
    definitions.append(makeDefinition("MEAS.LOAD_RATIO.CH01", u8"负载率 CH01", TagCategory::Derived, "%", opt(0), opt(100), opt(70), opt(85), std::nullopt, std::nullopt, true, u8"以电流量程 5A 计算", TagDataType::Double, TagValueKind::DerivedNumeric, true, 1000, 220));

    definitions.append(makeDefinition("MATRIX.LIGHT.AVG", u8"矩阵平均光强", TagCategory::Matrix, "lux", opt(0), opt(2000), opt(1200), opt(1600), opt(300), opt(100), true, u8"热力图概览指标", TagDataType::Double, TagValueKind::MatrixStat, true, 1000, 310));
    definitions.append(makeDefinition("MATRIX.LIGHT.MAX", u8"矩阵最大光强", TagCategory::Matrix, "lux", opt(0), opt(2500), opt(1500), opt(1800), std::nullopt, std::nullopt, true, u8"局部热点判断指标", TagDataType::Double, TagValueKind::MatrixStat, true, 1000, 320));
    definitions.append(makeDefinition("MATRIX.LIGHT.MIN", u8"矩阵最小光强", TagCategory::Matrix, "lux", opt(0), opt(2000), std::nullopt, std::nullopt, opt(100), opt(50), true, u8"局部低值判断指标", TagDataType::Double, TagValueKind::MatrixStat, true, 1000, 330));
    definitions.append(makeDefinition("MATRIX.LIGHT.UNIFORMITY", u8"矩阵均匀性", TagCategory::Matrix, "ratio", opt(0), opt(1), std::nullopt, std::nullopt, opt(0.70), opt(0.55), true, u8"越接近 1 越均匀", TagDataType::Double, TagValueKind::MatrixStat, true, 1000, 340));
    definitions.append(makeDefinition("MATRIX.LIGHT.ABNORMAL_COUNT", u8"矩阵异常点数量", TagCategory::Matrix, "points", opt(0), opt(256), opt(5), opt(20), std::nullopt, std::nullopt, true, u8"用于热点/暗区快速告警", TagDataType::Int, TagValueKind::MatrixStat, true, 1000, 350));
    definitions.append(makeDefinition("MATRIX.LIGHT.HOTSPOT_ROW", u8"热点行坐标", TagCategory::Matrix, "row", opt(0), opt(15), std::nullopt, std::nullopt, std::nullopt, std::nullopt, true, u8"辅助热力图定位", TagDataType::Int, TagValueKind::MatrixStat, false, 1000, 360));
    definitions.append(makeDefinition("MATRIX.LIGHT.HOTSPOT_COL", u8"热点列坐标", TagCategory::Matrix, "col", opt(0), opt(15), std::nullopt, std::nullopt, std::nullopt, std::nullopt, true, u8"辅助热力图定位", TagDataType::Int, TagValueKind::MatrixStat, false, 1000, 370));

    return definitions;
}

QVector<TagSourceMapping> defaultSourceMappings()
{
    QVector<TagSourceMapping> mappings;
    mappings.reserve(22);

    mappings.append(makeMapping("DEVICE.STATUS", SourceType::FrameField, "", "DeviceStatus"));
    mappings.append(makeMapping("DEVICE.ONLINE", SourceType::FrameField, "", "DeviceStatus"));
    mappings.append(makeMapping("DEVICE.ERROR_CODE", SourceType::FrameField, "", "ErrorCode"));
    mappings.append(makeMapping("DEVICE.QUALITY", SourceType::FrameField, "", "Quality"));
    mappings.append(makeMapping("DEVICE.SEQUENCE_NO", SourceType::FrameField, "", "SequenceNo"));
    mappings.append(makeMapping("DEVICE.FRAME_INTERVAL_MS", SourceType::Runtime, "", "TimestampDelta"));
    mappings.append(makeMapping("DEVICE.FRAME_LOSS_COUNT", SourceType::Runtime, "", "SequenceNoDelta"));

    mappings.append(makeMapping("MEAS.TEMP.CH01", SourceType::Channel, "TEMP_CH01"));
    mappings.append(makeMapping("MEAS.PRESSURE.CH01", SourceType::Channel, "PRESSURE_CH01"));
    mappings.append(makeMapping("MEAS.LIGHT.CH01", SourceType::Channel, "LIGHT_CH01"));
    mappings.append(makeMapping("MEAS.VOLTAGE.CH01", SourceType::Channel, "VOLTAGE_CH01"));
    mappings.append(makeMapping("MEAS.CURRENT.CH01", SourceType::Channel, "CURRENT_CH01"));
    mappings.append(makeMapping("MEAS.VIBRATION.CH01", SourceType::Channel, "VIBRATION_CH01"));

    mappings.append(makeMapping("MEAS.POWER.CH01", SourceType::Derived, "", "", "MEAS.VOLTAGE.CH01 * MEAS.CURRENT.CH01", "MEAS.VOLTAGE.CH01,MEAS.CURRENT.CH01"));
    mappings.append(makeMapping("MEAS.LOAD_RATIO.CH01", SourceType::Derived, "", "", "MEAS.CURRENT.CH01 / 5.0 * 100", "MEAS.CURRENT.CH01"));

    mappings.append(makeMapping("MATRIX.LIGHT.AVG", SourceType::Matrix, "", "Average(MatrixValues)"));
    mappings.append(makeMapping("MATRIX.LIGHT.MAX", SourceType::Matrix, "", "Max(MatrixValues)"));
    mappings.append(makeMapping("MATRIX.LIGHT.MIN", SourceType::Matrix, "", "Min(MatrixValues)"));
    mappings.append(makeMapping("MATRIX.LIGHT.UNIFORMITY", SourceType::Matrix, "", "Uniformity(MatrixValues)"));
    mappings.append(makeMapping("MATRIX.LIGHT.ABNORMAL_COUNT", SourceType::Matrix, "", "Count(MatrixValues outside warning range)"));
    mappings.append(makeMapping("MATRIX.LIGHT.HOTSPOT_ROW", SourceType::Matrix, "", "ArgMax.Row"));
    mappings.append(makeMapping("MATRIX.LIGHT.HOTSPOT_COL", SourceType::Matrix, "", "ArgMax.Col"));

    return mappings;
}

QVector<ModuleMappingItem> moduleMappingChecklist()
{
    return {
        {"Domain/Measurements", "RawFrame、Channel、Matrix、质量契约", "domain/measurements", "RawMeasurementFrame 字段必须完整对齐"},
        {"Domain/Tags", "Tag 定义、来源映射、运行值", "domain/tags", "TagId、质量、报警状态和值类型保持一致"},
        {"Domain/Alarms", "警报定义、事件和生命周期", "domain/alarms", "Active/Acknowledged/Recovered 语义保持一致"},
        {"Domain/Rules", "质量、阈值和矩阵统计规则", "domain/rules", "先迁移纯计算并复刻测试"},
        {"Application/Configuration", "运行参数与 Tag 动态配置", "application/configuration", "默认周期、保留期和趋势窗口固定"},
        {"Application/Services/TagDefinitionCatalog.cs", "默认 Tag 字典与来源映射", "application/services/TagDefinitionCatalog.*", "22 个 Tag 与 22 个 Mapping 逐项一致"},
        {"Application/Pipelines/DataCleanPipeline.cs", "RawFrame 到 CleanedTagValue", "application/pipelines/DataCleanPipeline.*", "派生量、质量传播和矩阵统计是第一验收点"},
        {"Application/Services/AlarmService.cs", "警报评估与生命周期", "application/services/AlarmService.*", "质量警报、阈值警报和确认恢复保持一致"},
        {"Application/Services/UiSnapshotProvider.cs", "UI 聚合快照", "application/services/UiSnapshotProvider.*", "Qt 页面只读快照"},
        {"Application/Queues + BackgroundWorkers", "历史、警报、日志队列和批量写入", "application/queues + application/workers", "5s 或 100 条触发，停止时最终 flush"},
        {"Infrastructure/Persistence", "SQLite schema、仓储和迁移", "infrastructure/persistence", "schema version 5，QtSql 每线程独立连接"},
        {"Simulator/Generators + Scenarios", "虚拟设备帧和异常场景", "simulator/generators + simulator/scenarios", "默认设备 MCMD-001，16x16 矩阵，120s Demo 循环"},
        {"MultiChannelMonitor/ViewModels", "页面状态和命令", "presentation/viewmodels 或 presenters", "Qt slot/QTimer/refresh(snapshot) 替代 Binding"},
        {"MultiChannelMonitor/Views", "七个 WPF 页面", "presentation/pages", "按页面能力重建 Qt Widgets，不逐行翻译 XAML"},
        {"MultiChannelMonitor/Renderers", "趋势和热力图渲染", "presentation/renderers", "趋势图与热力图只绘制快照，不计算业务"}
    };
}

QVector<PageFeatureChecklistItem> pageFeatureChecklist()
{
    return {
        {"Dashboard", "DashboardViewModel", "DashboardPageWidget", {"指标卡", "关键 Tag", "活跃警报", "趋势预览", "矩阵预览"}},
        {"Realtime Tags", "RealtimeTagsViewModel", "RealtimeTagsPageWidget", {"Tag 表格", "分类筛选", "文本筛选", "质量/报警状态", "跳转趋势"}},
        {"Trend", "TrendViewModel", "TrendPageWidget", {"Tag 选择", "1/5/30 分钟窗口", "实时曲线", "阈值线", "质量点", "尖峰诊断", "跳转历史"}},
        {"Alarm Center", "AlarmCenterViewModel", "AlarmCenterPageWidget", {"当前警报", "历史查询", "分页", "取消查询", "确认警报"}},
        {"History", "HistoryViewModel", "HistoryPageWidget", {"Tag/时间查询", "分页", "取消查询", "趋势预览", "CSV 导出"}},
        {"Measurement Map", "MeasurementMapViewModel", "MeasurementMapPageWidget", {"16x16 热力图", "自动/固定量程", "统计摘要", "异常点", "单元格选择"}},
        {"Logs & Settings", "LogsSettingsViewModel", "LogsSettingsPageWidget", {"业务日志查询", "Tag 阈值", "历史策略", "运行参数保存"}}
    };
}

QVector<TestReplicationChecklistItem> testReplicationChecklist()
{
    return {
        {"Domain", "Tests/DomainTests/MatrixFrameTests.cs", 6, "Must", "矩阵尺寸、统计和无效值行为"},
        {"Application", "Tests/ApplicationTests/AlarmEventPipelineTests.cs", 4, "Must", "警报事件入队和操作日志联动"},
        {"Application", "Tests/ApplicationTests/AlarmIteration6Tests.cs", 5, "Must", "警报生命周期、确认和恢复"},
        {"Application", "Tests/ApplicationTests/AlarmServiceTests.cs", 5, "Must", "质量/阈值警报评估"},
        {"Application", "Tests/ApplicationTests/ApplicationEventPublisherTests.cs", 5, "Must", "Critical/Isolated 事件处理失败策略"},
        {"Application", "Tests/ApplicationTests/ChartDataServiceTests.cs", 6, "Should", "趋势快照和窗口数据"},
        {"Application", "Tests/ApplicationTests/ConfigurationServiceTests.cs", 5, "Should", "运行参数和 Tag 设置保存"},
        {"Application", "Tests/ApplicationTests/DataCleanPipelineRawQualityTests.cs", 2, "Must", "DeviceError/Offline 质量传播"},
        {"Application", "Tests/ApplicationTests/DataPipelineTests.cs", 4, "Must", "RawFrame 到 Tag 处理链路"},
        {"Application", "Tests/ApplicationTests/DataSourceHealthMonitorTests.cs", 6, "Must", "1.5s 超时、Offline 和恢复"},
        {"Application", "Tests/ApplicationTests/HistoryIteration5Tests.cs", 4, "Must", "历史采样和保留策略"},
        {"Application", "Tests/ApplicationTests/HistoryPipelineTests.cs", 5, "Must", "历史队列和采样入库"},
        {"Application", "Tests/ApplicationTests/MeasurementMapAnalysisTests.cs", 14, "Must", "矩阵量程、热力、异常点和质量"},
        {"Application", "Tests/ApplicationTests/MeasurementMapPipelineTests.cs", 4, "Must", "矩阵帧消费和快照"},
        {"Application", "Tests/ApplicationTests/MonitorRuntimeOptionsTests.cs", 4, "Must", "500ms/1s/5s/30天/1-5-30分钟默认参数"},
        {"Application", "Tests/ApplicationTests/OperationLogPipelineTests.cs", 3, "Should", "业务操作日志事件链路"},
        {"Application", "Tests/ApplicationTests/PersistenceRuntimeCoordinatorTests.cs", 6, "Must", "持久化 Worker 启停和 flush"},
        {"Application", "Tests/ApplicationTests/PersistWorkerFailurePolicyTests.cs", 3, "Must", "批量写入失败降级策略"},
        {"Application", "Tests/ApplicationTests/RuntimeLifecycleCoordinatorTests.cs", 4, "Must", "Start/Stop/Faulted 生命周期"},
        {"Application", "Tests/ApplicationTests/TrendDiagnosisServiceTests.cs", 6, "Should", "趋势尖峰和诊断"},
        {"Application", "Tests/ApplicationTests/TrendStatisticsCalculatorTests.cs", 3, "Should", "趋势统计"},
        {"Application", "Tests/ApplicationTests/UiSnapshotProviderTests.cs", 5, "Must", "UI 组合快照一致性"},
        {"Application", "Tests/ApplicationTests/UtcTimeChainTests.cs", 4, "Must", "UTC 时间和 ticks 链路"},
        {"AppLogging", "Tests/AppLoggingTests/AppLoggerTests.cs", 2, "Could", "日志门面"},
        {"Infrastructure", "Tests/InfrastructureTests/AlarmIteration6RepositoryTests.cs", 3, "Must", "警报仓储查询与恢复"},
        {"Infrastructure", "Tests/InfrastructureTests/InMemoryHistoryRepositoryTests.cs", 1, "Should", "历史仓储契约"},
        {"Infrastructure", "Tests/InfrastructureTests/LoggingBootstrapperTests.cs", 1, "Could", "Serilog bootstrap 等价替换"},
        {"Infrastructure", "Tests/InfrastructureTests/SQLiteRepositoryTests.cs", 18, "Must", "SQLite schema v5、事务、分页和设置持久化"},
        {"Simulator", "Tests/SimulatorTests/FakeDataGeneratorTests.cs", 9, "Must", "默认设备、6 通道、16x16 矩阵、异常场景和 UTC"},
        {"Specification", "Tests/SpecificationTests/DataCleanPipelineMappingSpecTests.cs", 4, "Must", "RawCode 到业务 TagId、派生量和矩阵统计"},
        {"Specification", "Tests/SpecificationTests/RuntimeStateAndPersistenceSpecTests.cs", 4, "Must", "CleanedValue/RuntimeState/MatrixFrame 分离"},
        {"Specification", "Tests/SpecificationTests/TagBusinessDictionarySpecTests.cs", 2, "Must", "22 个 Tag 和 22 个 Mapping"}
    };
}

RawMeasurementFrame createMinimumAcceptanceFrame()
{
    RawMeasurementFrame frame;
    frame.frameId = QUuid(QStringLiteral("{00000000-0000-0000-0000-000000000101}"));
    frame.deviceId = defaultDeviceId();
    frame.sequenceNo = 1;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    frame.timestampUtc = QDateTime(QDate(2026, 6, 25), QTime(0, 0, 0), QTimeZone(QTimeZone::UTC));
#else
    frame.timestampUtc = QDateTime(QDate(2026, 6, 25), QTime(0, 0, 0), Qt::UTC);
#endif
    frame.deviceStatus = DeviceStatus::Running;
    frame.errorCode = 0;
    frame.quality = TagQuality::Good;

    frame.channelValues = {
        {QStringLiteral("TEMP_CH01"), 25.4, QString::fromUtf8(u8"℃"), TagQuality::Good, 0},
        {QStringLiteral("PRESSURE_CH01"), 101.2, QStringLiteral("kPa"), TagQuality::Good, 0},
        {QStringLiteral("LIGHT_CH01"), 580.6, QStringLiteral("lux"), TagQuality::Good, 0},
        {QStringLiteral("VOLTAGE_CH01"), 12.1, QStringLiteral("V"), TagQuality::Good, 0},
        {QStringLiteral("CURRENT_CH01"), 1.2, QStringLiteral("A"), TagQuality::Good, 0},
        {QStringLiteral("VIBRATION_CH01"), 0.03, QStringLiteral("mm/s"), TagQuality::Good, 0}
    };

    MatrixFrame matrix;
    matrix.frameId = QUuid(QStringLiteral("{00000000-0000-0000-0000-000000000201}"));
    matrix.timestampUtc = frame.timestampUtc;
    matrix.rows = 16;
    matrix.columns = 16;
    matrix.sourceFrameId = frame.frameId;
    matrix.sequenceNo = frame.sequenceNo;
    matrix.values.reserve(matrix.rows * matrix.columns);
    for (auto row = 0; row < matrix.rows; ++row) {
        for (auto column = 0; column < matrix.columns; ++column) {
            matrix.values.append(600.0 + row * 2.0 + column);
        }
    }

    frame.matrixValues = matrix;
    frame.hasMatrixValues = true;
    return frame;
}

AcceptanceEvaluation evaluateMinimumAcceptanceFrame()
{
    return evaluateFrame(createMinimumAcceptanceFrame());
}

AcceptanceEvaluation evaluateFrame(const RawMeasurementFrame &frame)
{
    const auto definitions = defaultTagDefinitions();
    const auto mappings = defaultSourceMappings();
    AcceptanceEvaluation evaluation;
    evaluation.frame = frame;

    if (frame.hasMatrixValues) {
        evaluation.matrixStatistics = calculateMatrixStatistics(frame.matrixValues);
        evaluation.matrixAbnormalPointCount = detectAbnormalPointCount(frame.matrixValues, evaluation.matrixStatistics);
    }

    appendFrameFieldTags(frame, definitions, mappings, &evaluation.cleanedValues);
    appendChannelTags(frame, definitions, mappings, &evaluation.cleanedValues);
    appendDerivedTags(frame, definitions, mappings, &evaluation.cleanedValues);
    appendMatrixStatisticTags(frame,
                              definitions,
                              mappings,
                              evaluation.matrixStatistics,
                              evaluation.matrixAbnormalPointCount,
                              &evaluation.cleanedValues);

    for (auto &value : evaluation.cleanedValues) {
        const auto *definition = findDefinition(definitions, value.tagId);
        value.alarmState = evaluateAlarmState(numericValueForAlarm(value), value.quality, definition);
    }

    evaluation.historySampleCandidateTagIds = historizedTagIds(definitions, evaluation.cleanedValues);
    return evaluation;
}

bool validateSourceBehaviorFreeze(QStringList *errors)
{
    QStringList localErrors;
    if (!errors) {
        errors = &localErrors;
    }

    errors->clear();
    const auto options = defaultRuntimeOptions();
    if (options.dataGenerateIntervalMs != 500 ||
        options.dataSourceTimeoutPeriods != 3 ||
        options.dataSourceTimeoutMs() != 1500 ||
        options.uiRefreshIntervalMs != 1000 ||
        options.historyBatchIntervalMs != 5000 ||
        options.historyRetentionDays != 30 ||
        options.alarmBatchIntervalMs != 5000 ||
        options.operationLogBatchIntervalMs != 5000) {
        addError(errors, QStringLiteral("Default runtime cadence does not match the WPF source project."));
    }

    if (options.trendPointCount(1) != 120 ||
        options.trendPointCount(5) != 600 ||
        options.trendPointCount(30) != 3600 ||
        options.trendBufferCapacity() != 3600) {
        addError(errors, QStringLiteral("Trend window capacity is not aligned with 500 ms sampling."));
    }

    const auto definitions = defaultTagDefinitions();
    if (definitions.size() != 22) {
        addError(errors, QStringLiteral("Default TagDefinition count must stay at 22."));
    }

    QSet<QString> tagIds;
    for (const auto &definition : definitions) {
        if (tagIds.contains(definition.tagId)) {
            addError(errors, QStringLiteral("Duplicate TagDefinition: %1").arg(definition.tagId));
        }
        tagIds.insert(definition.tagId);
    }

    const QStringList requiredTagIds = {
        QStringLiteral("DEVICE.STATUS"),
        QStringLiteral("DEVICE.ONLINE"),
        QStringLiteral("DEVICE.ERROR_CODE"),
        QStringLiteral("DEVICE.QUALITY"),
        QStringLiteral("DEVICE.SEQUENCE_NO"),
        QStringLiteral("DEVICE.FRAME_INTERVAL_MS"),
        QStringLiteral("DEVICE.FRAME_LOSS_COUNT"),
        QStringLiteral("MEAS.TEMP.CH01"),
        QStringLiteral("MEAS.PRESSURE.CH01"),
        QStringLiteral("MEAS.LIGHT.CH01"),
        QStringLiteral("MEAS.VOLTAGE.CH01"),
        QStringLiteral("MEAS.CURRENT.CH01"),
        QStringLiteral("MEAS.VIBRATION.CH01"),
        QStringLiteral("MEAS.POWER.CH01"),
        QStringLiteral("MEAS.LOAD_RATIO.CH01"),
        QStringLiteral("MATRIX.LIGHT.AVG"),
        QStringLiteral("MATRIX.LIGHT.MAX"),
        QStringLiteral("MATRIX.LIGHT.MIN"),
        QStringLiteral("MATRIX.LIGHT.UNIFORMITY"),
        QStringLiteral("MATRIX.LIGHT.ABNORMAL_COUNT"),
        QStringLiteral("MATRIX.LIGHT.HOTSPOT_ROW"),
        QStringLiteral("MATRIX.LIGHT.HOTSPOT_COL")
    };

    for (const auto &tagId : requiredTagIds) {
        if (!tagIds.contains(tagId)) {
            addError(errors, QStringLiteral("Required TagDefinition is missing: %1").arg(tagId));
        }
    }

    const auto mappings = defaultSourceMappings();
    if (mappings.size() != 22) {
        addError(errors, QStringLiteral("Default TagSourceMapping count must stay at 22."));
    }

    const QHash<QString, QString> channelMappings = {
        {QStringLiteral("TEMP_CH01"), QStringLiteral("MEAS.TEMP.CH01")},
        {QStringLiteral("PRESSURE_CH01"), QStringLiteral("MEAS.PRESSURE.CH01")},
        {QStringLiteral("LIGHT_CH01"), QStringLiteral("MEAS.LIGHT.CH01")},
        {QStringLiteral("VOLTAGE_CH01"), QStringLiteral("MEAS.VOLTAGE.CH01")},
        {QStringLiteral("CURRENT_CH01"), QStringLiteral("MEAS.CURRENT.CH01")},
        {QStringLiteral("VIBRATION_CH01"), QStringLiteral("MEAS.VIBRATION.CH01")}
    };

    for (auto it = channelMappings.cbegin(); it != channelMappings.cend(); ++it) {
        auto found = false;
        for (const auto &mapping : mappings) {
            if (mapping.sourceType == SourceType::Channel &&
                mapping.sourceCode == it.key() &&
                mapping.tagId == it.value()) {
                found = true;
                break;
            }
        }

        if (!found) {
            addError(errors, QStringLiteral("Required channel mapping is missing: %1 -> %2").arg(it.key(), it.value()));
        }
    }

    const auto evaluation = evaluateMinimumAcceptanceFrame();
    if (evaluation.frame.deviceId != defaultDeviceId() ||
        evaluation.frame.channelValues.size() != 6 ||
        !evaluation.frame.hasMatrixValues ||
        evaluation.frame.matrixValues.rows != 16 ||
        evaluation.frame.matrixValues.columns != 16 ||
        evaluation.frame.timestampUtc.timeSpec() != Qt::UTC) {
        addError(errors, QStringLiteral("Minimum acceptance RawFrame is not aligned with the WPF simulator contract."));
    }

    if (evaluation.cleanedValues.size() != 22) {
        addError(errors, QStringLiteral("Minimum acceptance frame must produce 22 cleaned tag values."));
    }

    const auto indexes = tagIndexes(evaluation.cleanedValues);
    const auto requireNumeric = [&](const QString &tagId, double expected, const QString &message) {
        if (!indexes.contains(tagId)) {
            addError(errors, QStringLiteral("Missing evaluated tag: %1").arg(tagId));
            return;
        }

        const auto &value = evaluation.cleanedValues.at(indexes.value(tagId));
        if (!value.numericValue.has_value() || !nearlyEqual(value.numericValue.value(), expected)) {
            addError(errors, message);
        }

        if (value.quality != TagQuality::Good || value.alarmState != TagAlarmState::Normal) {
            addError(errors, QStringLiteral("Expected normal good tag state for %1.").arg(tagId));
        }
    };

    requireNumeric(QStringLiteral("MEAS.POWER.CH01"), 14.52, QStringLiteral("Power derived tag must equal voltage * current."));
    requireNumeric(QStringLiteral("MEAS.LOAD_RATIO.CH01"), 24.0, QStringLiteral("Load ratio derived tag must equal current / 5A * 100."));
    requireNumeric(QStringLiteral("MATRIX.LIGHT.AVG"), 622.5, QStringLiteral("Matrix average statistic is not stable."));
    requireNumeric(QStringLiteral("MATRIX.LIGHT.MAX"), 645.0, QStringLiteral("Matrix max statistic is not stable."));
    requireNumeric(QStringLiteral("MATRIX.LIGHT.MIN"), 600.0, QStringLiteral("Matrix min statistic is not stable."));
    requireNumeric(QStringLiteral("MATRIX.LIGHT.UNIFORMITY"), 600.0 / 645.0, QStringLiteral("Matrix uniformity statistic is not stable."));
    requireNumeric(QStringLiteral("MATRIX.LIGHT.ABNORMAL_COUNT"), 0.0, QStringLiteral("Minimum matrix frame should not contain abnormal points."));
    requireNumeric(QStringLiteral("MATRIX.LIGHT.HOTSPOT_ROW"), 15.0, QStringLiteral("Matrix hotspot row should be 15."));
    requireNumeric(QStringLiteral("MATRIX.LIGHT.HOTSPOT_COL"), 15.0, QStringLiteral("Matrix hotspot column should be 15."));

    if (!evaluation.historySampleCandidateTagIds.contains(QStringLiteral("MEAS.TEMP.CH01")) ||
        !evaluation.historySampleCandidateTagIds.contains(QStringLiteral("MATRIX.LIGHT.AVG")) ||
        evaluation.historySampleCandidateTagIds.contains(QStringLiteral("DEVICE.STATUS"))) {
        addError(errors, QStringLiteral("History sample candidate list does not match Tag historization flags."));
    }

    auto totalTests = 0;
    for (const auto &item : testReplicationChecklist()) {
        totalTests += item.testCount;
    }

    if (totalTests != 157) {
        addError(errors, QStringLiteral("C# test replication inventory must stay at 157 Fact/Theory cases."));
    }

    if (pageFeatureChecklist().size() != 7) {
        addError(errors, QStringLiteral("Page parity checklist must cover exactly seven WPF pages."));
    }

    return errors->isEmpty();
}

QString toString(DeviceStatus status)
{
    switch (status) {
    case DeviceStatus::Stopped:
        return QStringLiteral("Stopped");
    case DeviceStatus::Running:
        return QStringLiteral("Running");
    case DeviceStatus::Warning:
        return QStringLiteral("Warning");
    case DeviceStatus::Error:
        return QStringLiteral("Error");
    case DeviceStatus::Offline:
        return QStringLiteral("Offline");
    }

    return QString();
}

QString toString(TagQuality quality)
{
    switch (quality) {
    case TagQuality::Good:
        return QStringLiteral("Good");
    case TagQuality::Bad:
        return QStringLiteral("Bad");
    case TagQuality::Timeout:
        return QStringLiteral("Timeout");
    case TagQuality::OutOfRange:
        return QStringLiteral("OutOfRange");
    case TagQuality::DeviceError:
        return QStringLiteral("DeviceError");
    case TagQuality::Offline:
        return QStringLiteral("Offline");
    }

    return QString();
}

QString toString(TagAlarmState state)
{
    switch (state) {
    case TagAlarmState::Normal:
        return QStringLiteral("Normal");
    case TagAlarmState::WarningHigh:
        return QStringLiteral("WarningHigh");
    case TagAlarmState::WarningLow:
        return QStringLiteral("WarningLow");
    case TagAlarmState::AlarmHigh:
        return QStringLiteral("AlarmHigh");
    case TagAlarmState::AlarmLow:
        return QStringLiteral("AlarmLow");
    case TagAlarmState::Invalid:
        return QStringLiteral("Invalid");
    case TagAlarmState::Offline:
        return QStringLiteral("Offline");
    }

    return QString();
}

} // namespace Phase0
