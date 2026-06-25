#include "DataCleanPipeline.h"

#include "application/services/MeasurementMapAnalysis.h"
#include "application/services/TagDefinitionCatalog.h"
#include "domain/alarms/AlarmModels.h"
#include "domain/common/DomainCommon.h"
#include "domain/devices/DeviceModels.h"
#include "domain/rules/DomainRules.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace Monitor::Application::Pipelines {
namespace {

using Monitor::Domain::Devices::DeviceStatus;
using Monitor::Domain::Measurements::ChannelValue;
using Monitor::Domain::Measurements::RawMeasurementFrame;
using Monitor::Domain::Tags::CleanedTagValue;
using Monitor::Domain::Tags::SourceType;
using Monitor::Domain::Tags::TagAlarmState;
using Monitor::Domain::Tags::TagDefinition;
using Monitor::Domain::Tags::TagQuality;
using Monitor::Domain::Tags::TagRuntimeState;
using Monitor::Domain::Tags::TagSourceMapping;

TagQuality applyRangeQuality(double value, TagQuality quality, const TagDefinition &definition)
{
    if (quality != TagQuality::Good) {
        return quality;
    }

    return Monitor::Domain::Rules::TagValidationRule::validateRange(value, definition);
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

const TagDefinition *findDefinition(
    const QHash<QString, TagDefinition> &definitions,
    const QString &tagId)
{
    const auto it = definitions.constFind(tagId);
    return it == definitions.cend() ? nullptr : &it.value();
}

const TagSourceMapping *findMapping(
    const QVector<TagSourceMapping> &mappings,
    const QString &tagId)
{
    const auto it = std::find_if(mappings.cbegin(), mappings.cend(), [&tagId](const auto &mapping) {
        return mapping.tagId == tagId;
    });

    return it == mappings.cend() ? nullptr : &(*it);
}

std::optional<double> numericEquivalent(const CleanedTagValue &value)
{
    if (value.numericValue.has_value()) {
        return value.numericValue;
    }

    if (value.boolValue.has_value()) {
        return value.boolValue.value() ? 1.0 : 0.0;
    }

    return std::nullopt;
}

std::pair<int, int> findHotspot(const Monitor::Domain::Measurements::MatrixFrame &matrix)
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

} // namespace

DataCleanPipeline::DataCleanPipeline()
    : DataCleanPipeline(Services::TagDefinitionCatalog::createDefaults())
{
}

DataCleanPipeline::DataCleanPipeline(const QVector<TagDefinition> &definitions)
    : DataCleanPipeline(definitions, Services::TagDefinitionCatalog::createSourceMappings())
{
}

DataCleanPipeline::DataCleanPipeline(
    const QVector<TagDefinition> &definitions,
    const QVector<TagSourceMapping> &mappings)
{
    for (const auto &definition : definitions) {
        m_definitions.insert(definition.tagId, definition);
    }

    m_mappings.reserve(mappings.size());
    for (const auto &mapping : mappings) {
        if (mapping.isEnabled) {
            m_mappings.append(mapping);
        }
    }
}

QVector<Monitor::Domain::Tags::TagValue> DataCleanPipeline::clean(
    const RawMeasurementFrame &frame)
{
    QVector<Monitor::Domain::Tags::TagValue> result;
    const auto cleanedValues = cleanToCleanedValues(frame);
    result.reserve(cleanedValues.size());

    for (const auto &value : cleanedValues) {
        const auto numericValue = value.numericValue.value_or(
            value.boolValue.has_value()
                ? (value.boolValue.value() ? 1.0 : 0.0)
                : 0.0);
        result.append(Monitor::Domain::Tags::TagValue{
            value.tagId,
            numericValue,
            value.timestampUtc,
            value.quality,
            evaluateAlarmState(numericValue, value.quality, findDefinition(m_definitions, value.tagId)),
            value.sourceDeviceId,
            value.sequenceNo
        });
    }

    return result;
}

QVector<CleanedTagValue> DataCleanPipeline::cleanToCleanedValues(
    const RawMeasurementFrame &frame)
{
    Monitor::Domain::Measurements::MeasurementTimeContract::validate(frame);

    QVector<CleanedTagValue> values;
    values.reserve(m_mappings.size());
    addFrameFieldTags(frame, &values);
    addChannelTags(frame, &values);
    addDerivedTags(frame, &values);
    addMatrixStatisticTags(frame, &values);

    m_lastFrames.insert(frame.deviceId, {frame.timestampUtc, frame.sequenceNo});
    return values;
}

QVector<TagRuntimeState> DataCleanPipeline::toRuntimeStates(
    const RawMeasurementFrame &frame,
    const QDateTime &lastUpdateTimeUtc)
{
    Monitor::Domain::Common::UtcDateTime::require(lastUpdateTimeUtc, QStringLiteral("lastUpdateTimeUtc"));

    QVector<TagRuntimeState> result;
    const auto cleanedValues = cleanToCleanedValues(frame);
    result.reserve(cleanedValues.size());

    for (const auto &value : cleanedValues) {
        const auto *definition = findDefinition(m_definitions, value.tagId);
        const auto numericValue = numericEquivalent(value);
        result.append(TagRuntimeState{
            value.tagId,
            definition ? definition->displayName : value.tagId,
            definition ? definition->category : Monitor::Domain::Tags::TagCategory::Runtime,
            value.numericValue,
            value.textValue,
            value.boolValue,
            definition ? std::optional<QString>(definition->unit) : value.unit,
            definition ? definition->dataType : value.dataType,
            value.quality,
            evaluateAlarmState(numericValue.value_or(0.0), value.quality, definition),
            value.timestampUtc,
            value.sourceFrameId,
            value.sequenceNo,
            lastUpdateTimeUtc
        });
    }

    return result;
}

void DataCleanPipeline::resetSession()
{
    m_lastFrames.clear();
}

void DataCleanPipeline::addFrameFieldTags(
    const RawMeasurementFrame &frame,
    QVector<CleanedTagValue> *values) const
{
    for (const auto &mapping : m_mappings) {
        if (mapping.sourceType != SourceType::FrameField && mapping.sourceType != SourceType::Runtime) {
            continue;
        }

        const auto *definition = findDefinition(m_definitions, mapping.tagId);
        if (!definition) {
            continue;
        }

        auto quality = frame.deviceStatus == DeviceStatus::Offline
            ? TagQuality::Offline
            : frame.quality;
        const auto sourcePath = mapping.sourcePath.value_or(QString());
        std::optional<double> numericValue;
        std::optional<QString> textValue;
        std::optional<bool> boolValue;

        if (sourcePath == QStringLiteral("DeviceStatus")) {
            if (mapping.tagId == QStringLiteral("DEVICE.ONLINE")) {
                boolValue = frame.deviceStatus != DeviceStatus::Offline;
            } else {
                textValue = Monitor::Domain::Devices::toString(frame.deviceStatus);
            }
        } else if (sourcePath == QStringLiteral("ErrorCode")) {
            numericValue = static_cast<double>(frame.errorCode);
        } else if (sourcePath == QStringLiteral("Quality")) {
            textValue = Monitor::Domain::Tags::toString(frame.quality);
        } else if (sourcePath == QStringLiteral("SequenceNo")) {
            numericValue = static_cast<double>(frame.sequenceNo);
        } else if (sourcePath == QStringLiteral("TimestampDelta")) {
            const auto it = m_lastFrames.constFind(frame.deviceId);
            numericValue = it == m_lastFrames.cend()
                ? 0.0
                : static_cast<double>(std::max<qint64>(0, it.value().timestampUtc.msecsTo(frame.timestampUtc)));
        } else if (sourcePath == QStringLiteral("SequenceNoDelta")) {
            const auto it = m_lastFrames.constFind(frame.deviceId);
            numericValue = it == m_lastFrames.cend()
                ? 0.0
                : static_cast<double>(std::max<qint64>(0, frame.sequenceNo - it.value().sequenceNo - 1));
        } else {
            continue;
        }

        values->append(createCleanedValue(frame, *definition, mapping, numericValue, textValue, boolValue, quality, std::nullopt));
    }
}

void DataCleanPipeline::addChannelTags(
    const RawMeasurementFrame &frame,
    QVector<CleanedTagValue> *values) const
{
    QHash<QString, ChannelValue> channelMap;
    for (const auto &channel : frame.channelValues) {
        channelMap.insert(channel.channelId, channel);
    }

    for (const auto &mapping : m_mappings) {
        if (mapping.sourceType != SourceType::Channel || !mapping.sourceCode.has_value()) {
            continue;
        }

        const auto *definition = findDefinition(m_definitions, mapping.tagId);
        if (!definition) {
            continue;
        }

        const auto channelIt = channelMap.constFind(mapping.sourceCode.value());
        if (channelIt == channelMap.cend()) {
            continue;
        }

        const auto &channel = channelIt.value();
        const auto valueIsMissing = std::isnan(channel.value);
        const std::optional<double> numericValue = valueIsMissing
            ? std::nullopt
            : std::optional<double>(channel.value * mapping.scale + mapping.offset);
        const auto quality = evaluateChannelQuality(frame, channel, *definition, numericValue);
        const std::optional<QString> cleanMessage = valueIsMissing
            ? std::optional<QString>(QStringLiteral("Raw channel value is missing."))
            : std::nullopt;

        values->append(createCleanedValue(frame, *definition, mapping, numericValue, std::nullopt, std::nullopt, quality, cleanMessage));
    }
}

void DataCleanPipeline::addDerivedTags(
    const RawMeasurementFrame &frame,
    QVector<CleanedTagValue> *values) const
{
    QHash<QString, CleanedTagValue> byTagId;
    for (const auto &value : *values) {
        byTagId.insert(value.tagId, value);
    }

    const auto addDerivedNumeric = [this, &frame, values, &byTagId](
        const QString &tagId,
        const QVector<QString> &inputTagIds,
        const std::function<double(const QVector<double> &)> &calculate) {
        const auto *definition = findDefinition(m_definitions, tagId);
        const auto *mapping = findMapping(m_mappings, tagId);
        if (!definition || !mapping) {
            return;
        }

        QVector<CleanedTagValue> inputs;
        QVector<double> inputValues;
        inputs.reserve(inputTagIds.size());
        inputValues.reserve(inputTagIds.size());
        auto hasOfflineInput = false;
        auto allInputsGood = true;

        for (const auto &inputTagId : inputTagIds) {
            const auto inputIt = byTagId.constFind(inputTagId);
            if (inputIt == byTagId.cend()) {
                allInputsGood = false;
                continue;
            }

            inputs.append(inputIt.value());
            hasOfflineInput = hasOfflineInput || inputIt.value().quality == TagQuality::Offline;
            allInputsGood = allInputsGood && inputIt.value().quality == TagQuality::Good && inputIt.value().numericValue.has_value();
            if (inputIt.value().numericValue.has_value()) {
                inputValues.append(inputIt.value().numericValue.value());
            }
        }

        auto quality = hasOfflineInput
            ? TagQuality::Offline
            : (allInputsGood && inputValues.size() == inputTagIds.size() ? TagQuality::Good : TagQuality::Bad);
        std::optional<double> numericValue;
        if (quality == TagQuality::Good) {
            numericValue = calculate(inputValues);
            quality = applyRangeQuality(numericValue.value(), quality, *definition);
        }

        values->append(createCleanedValue(frame, *definition, *mapping, numericValue, std::nullopt, std::nullopt, quality, std::nullopt));
        byTagId.insert(tagId, values->last());
    };

    addDerivedNumeric(
        QStringLiteral("MEAS.POWER.CH01"),
        {QStringLiteral("MEAS.VOLTAGE.CH01"), QStringLiteral("MEAS.CURRENT.CH01")},
        [](const QVector<double> &input) {
            return input.at(0) * input.at(1);
        });

    addDerivedNumeric(
        QStringLiteral("MEAS.LOAD_RATIO.CH01"),
        {QStringLiteral("MEAS.CURRENT.CH01")},
        [](const QVector<double> &input) {
            return input.at(0) / 5.0 * 100.0;
        });
}

void DataCleanPipeline::addMatrixStatisticTags(
    const RawMeasurementFrame &frame,
    QVector<CleanedTagValue> *values) const
{
    if (!frame.matrixValues.has_value()) {
        return;
    }

    const auto &matrix = frame.matrixValues.value();
    const auto statistics = matrix.calculateStatistics();
    const Services::MeasurementMap::AbnormalPointDetector abnormalPointDetector;
    const auto abnormalPoints = abnormalPointDetector.detect(matrix, statistics);
    const auto hotspot = findHotspot(matrix);

    QHash<QString, double> stats;
    stats.insert(QStringLiteral("MATRIX.LIGHT.AVG"), statistics.averageValue);
    stats.insert(QStringLiteral("MATRIX.LIGHT.MAX"), statistics.maxValue);
    stats.insert(QStringLiteral("MATRIX.LIGHT.MIN"), statistics.minValue);
    stats.insert(QStringLiteral("MATRIX.LIGHT.UNIFORMITY"), statistics.uniformity());
    stats.insert(QStringLiteral("MATRIX.LIGHT.ABNORMAL_COUNT"), static_cast<double>(abnormalPoints.size()));
    stats.insert(QStringLiteral("MATRIX.LIGHT.HOTSPOT_ROW"), static_cast<double>(hotspot.first));
    stats.insert(QStringLiteral("MATRIX.LIGHT.HOTSPOT_COL"), static_cast<double>(hotspot.second));

    for (const auto &mapping : m_mappings) {
        if (mapping.sourceType != SourceType::Matrix) {
            continue;
        }

        const auto *definition = findDefinition(m_definitions, mapping.tagId);
        const auto statIt = stats.constFind(mapping.tagId);
        if (!definition || statIt == stats.cend()) {
            continue;
        }

        auto quality = frame.deviceStatus == DeviceStatus::Offline
            ? TagQuality::Offline
            : frame.quality;
        quality = applyRangeQuality(statIt.value(), quality, *definition);
        values->append(createCleanedValue(frame, *definition, mapping, statIt.value(), std::nullopt, std::nullopt, quality, std::nullopt));
    }
}

CleanedTagValue DataCleanPipeline::createCleanedValue(
    const RawMeasurementFrame &frame,
    const TagDefinition &definition,
    const TagSourceMapping &mapping,
    std::optional<double> numericValue,
    std::optional<QString> textValue,
    std::optional<bool> boolValue,
    TagQuality quality,
    std::optional<QString> cleanMessage) const
{
    return {
        definition.tagId,
        numericValue,
        textValue,
        boolValue,
        definition.dataType,
        definition.unit,
        frame.timestampUtc,
        quality,
        frame.deviceId,
        mapping.sourceCode,
        frame.frameId,
        frame.sequenceNo,
        cleanMessage
    };
}

TagAlarmState DataCleanPipeline::evaluateAlarmState(
    double value,
    TagQuality quality,
    const TagDefinition *definition) const
{
    if (!definition) {
        if (quality == TagQuality::Offline) {
            return TagAlarmState::Offline;
        }
        return quality == TagQuality::Good ? TagAlarmState::Normal : TagAlarmState::Invalid;
    }

    return Monitor::Domain::Rules::AlarmRule::evaluate(
        value,
        quality,
        Monitor::Domain::Alarms::AlarmDefinition::fromTagDefinition(*definition));
}

} // namespace Monitor::Application::Pipelines
