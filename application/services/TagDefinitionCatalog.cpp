#include "TagDefinitionCatalog.h"

namespace Monitor::Application::Services {
namespace {

using Monitor::Domain::Tags::SourceType;
using Monitor::Domain::Tags::TagCategory;
using Monitor::Domain::Tags::TagDataType;
using Monitor::Domain::Tags::TagDefinition;
using Monitor::Domain::Tags::TagSourceMapping;
using Monitor::Domain::Tags::TagValueKind;

std::optional<double> opt(double value)
{
    return std::optional<double>(value);
}

QString text(const char *value)
{
    return QString::fromUtf8(value);
}

TagDefinition definition(
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
    return {
        text(tagId),
        text(displayName),
        category,
        text(unit),
        minValue,
        maxValue,
        warningHigh,
        alarmHigh,
        warningLow,
        alarmLow,
        isEnabled,
        text(description),
        dataType,
        valueKind,
        isHistorized,
        historyIntervalMs,
        displayOrder
    };
}

TagSourceMapping mapping(
    const char *tagId,
    const QString &sourceDeviceId,
    SourceType sourceType,
    const char *sourceCode = "",
    const char *sourcePath = "",
    const char *formula = "",
    const char *inputTagIds = "")
{
    TagSourceMapping result;
    result.tagId = text(tagId);
    result.sourceDeviceId = sourceDeviceId;
    result.sourceType = sourceType;
    result.sourceCode = sourceCode[0] == '\0' ? std::optional<QString>() : text(sourceCode);
    result.sourcePath = sourcePath[0] == '\0' ? std::optional<QString>() : text(sourcePath);
    result.formula = formula[0] == '\0' ? std::optional<QString>() : text(formula);
    result.inputTagIds = inputTagIds[0] == '\0' ? std::optional<QString>() : text(inputTagIds);
    return result;
}

} // namespace

const QString &TagDefinitionCatalog::defaultDeviceId()
{
    static const auto id = QStringLiteral("MCMD-001");
    return id;
}

QVector<TagDefinition> TagDefinitionCatalog::createDefaults()
{
    QVector<TagDefinition> definitions;
    definitions.reserve(22);

    definitions.append(definition("DEVICE.STATUS", u8"设备运行状态", TagCategory::Device, "", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, true, u8"设备当前运行状态", TagDataType::Enum, TagValueKind::Enum, false, 1000, 10));
    definitions.append(definition("DEVICE.ONLINE", u8"设备在线状态", TagCategory::Device, "", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, true, u8"由 DeviceStatus != Offline 推导", TagDataType::Boolean, TagValueKind::Boolean, true, 1000, 20));
    definitions.append(definition("DEVICE.ERROR_CODE", u8"设备错误码", TagCategory::Device, "", opt(0), opt(9999), std::nullopt, std::nullopt, std::nullopt, std::nullopt, true, u8"0 表示无错误", TagDataType::Int, TagValueKind::Numeric, true, 1000, 30));
    definitions.append(definition("DEVICE.QUALITY", u8"设备帧质量", TagCategory::Device, "", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, true, u8"Raw frame quality 映射", TagDataType::Enum, TagValueKind::Enum, true, 1000, 40));
    definitions.append(definition("DEVICE.SEQUENCE_NO", u8"最新帧序号", TagCategory::Device, "", opt(0), std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, true, u8"设备帧序号", TagDataType::Int, TagValueKind::Numeric, false, 1000, 50));
    definitions.append(definition("DEVICE.FRAME_INTERVAL_MS", u8"帧间隔", TagCategory::Runtime, "ms", opt(0), opt(5000), opt(750), opt(1500), std::nullopt, std::nullopt, true, u8"根据连续帧 Timestamp 计算", TagDataType::Double, TagValueKind::Numeric, true, 1000, 60));
    definitions.append(definition("DEVICE.FRAME_LOSS_COUNT", u8"连续丢帧数", TagCategory::Runtime, "frames", opt(0), std::nullopt, opt(1), opt(3), std::nullopt, std::nullopt, true, u8"根据 SequenceNo 跳号计算", TagDataType::Int, TagValueKind::Numeric, true, 1000, 70));

    definitions.append(definition("MEAS.TEMP.CH01", u8"温度 CH01", TagCategory::Measurement, u8"℃", opt(-20), opt(120), opt(60), opt(80), opt(5), opt(0), true, u8"温度超限演示主 Tag", TagDataType::Double, TagValueKind::Numeric, true, 1000, 110));
    definitions.append(definition("MEAS.PRESSURE.CH01", u8"压力 CH01", TagCategory::Measurement, "kPa", opt(80), opt(130), opt(115), opt(125), opt(90), opt(85), true, u8"压力稳定性监控", TagDataType::Double, TagValueKind::Numeric, true, 1000, 120));
    definitions.append(definition("MEAS.LIGHT.CH01", u8"光强 CH01", TagCategory::Measurement, "lux", opt(0), opt(2000), opt(1500), opt(1800), opt(100), opt(50), true, u8"光强单点监控", TagDataType::Double, TagValueKind::Numeric, true, 1000, 130));
    definitions.append(definition("MEAS.VOLTAGE.CH01", u8"电压 CH01", TagCategory::Electrical, "V", opt(0), opt(30), opt(14), opt(15), opt(10.5), opt(9.5), true, u8"电压跌落演示 Tag", TagDataType::Double, TagValueKind::Numeric, true, 1000, 140));
    definitions.append(definition("MEAS.CURRENT.CH01", u8"电流 CH01", TagCategory::Electrical, "A", opt(0), opt(5), opt(3), opt(4), opt(0.2), opt(0.1), true, u8"负载电流监控", TagDataType::Double, TagValueKind::Numeric, true, 1000, 150));
    definitions.append(definition("MEAS.VIBRATION.CH01", u8"振动 CH01", TagCategory::Mechanical, "mm/s", opt(0), opt(10), opt(1.0), opt(2.5), std::nullopt, std::nullopt, true, u8"振动尖峰演示 Tag", TagDataType::Double, TagValueKind::Numeric, true, 1000, 160));

    definitions.append(definition("MEAS.POWER.CH01", u8"功率 CH01", TagCategory::Derived, "W", opt(0), opt(150), opt(36), opt(48), std::nullopt, std::nullopt, true, u8"电压与电流派生功率", TagDataType::Double, TagValueKind::DerivedNumeric, true, 1000, 210));
    definitions.append(definition("MEAS.LOAD_RATIO.CH01", u8"负载率 CH01", TagCategory::Derived, "%", opt(0), opt(100), opt(70), opt(85), std::nullopt, std::nullopt, true, u8"以电流量程 5A 计算", TagDataType::Double, TagValueKind::DerivedNumeric, true, 1000, 220));

    definitions.append(definition("MATRIX.LIGHT.AVG", u8"矩阵平均光强", TagCategory::Matrix, "lux", opt(0), opt(2000), opt(1200), opt(1600), opt(300), opt(100), true, u8"热力图概览指标", TagDataType::Double, TagValueKind::MatrixStat, true, 1000, 310));
    definitions.append(definition("MATRIX.LIGHT.MAX", u8"矩阵最大光强", TagCategory::Matrix, "lux", opt(0), opt(2500), opt(1500), opt(1800), std::nullopt, std::nullopt, true, u8"局部热点判断指标", TagDataType::Double, TagValueKind::MatrixStat, true, 1000, 320));
    definitions.append(definition("MATRIX.LIGHT.MIN", u8"矩阵最小光强", TagCategory::Matrix, "lux", opt(0), opt(2000), std::nullopt, std::nullopt, opt(100), opt(50), true, u8"局部低值判断指标", TagDataType::Double, TagValueKind::MatrixStat, true, 1000, 330));
    definitions.append(definition("MATRIX.LIGHT.UNIFORMITY", u8"矩阵均匀性", TagCategory::Matrix, "ratio", opt(0), opt(1), std::nullopt, std::nullopt, opt(0.70), opt(0.55), true, u8"越接近 1 越均匀", TagDataType::Double, TagValueKind::MatrixStat, true, 1000, 340));
    definitions.append(definition("MATRIX.LIGHT.ABNORMAL_COUNT", u8"矩阵异常点数量", TagCategory::Matrix, "points", opt(0), opt(256), opt(5), opt(20), std::nullopt, std::nullopt, true, u8"用于热点/暗区快速告警", TagDataType::Int, TagValueKind::MatrixStat, true, 1000, 350));
    definitions.append(definition("MATRIX.LIGHT.HOTSPOT_ROW", u8"热点行坐标", TagCategory::Matrix, "row", opt(0), opt(15), std::nullopt, std::nullopt, std::nullopt, std::nullopt, true, u8"辅助热力图定位", TagDataType::Int, TagValueKind::MatrixStat, false, 1000, 360));
    definitions.append(definition("MATRIX.LIGHT.HOTSPOT_COL", u8"热点列坐标", TagCategory::Matrix, "col", opt(0), opt(15), std::nullopt, std::nullopt, std::nullopt, std::nullopt, true, u8"辅助热力图定位", TagDataType::Int, TagValueKind::MatrixStat, false, 1000, 370));

    return definitions;
}

QVector<TagSourceMapping> TagDefinitionCatalog::createSourceMappings(const QString &sourceDeviceId)
{
    QVector<TagSourceMapping> mappings;
    mappings.reserve(22);

    mappings.append(mapping("DEVICE.STATUS", sourceDeviceId, SourceType::FrameField, "", "DeviceStatus"));
    mappings.append(mapping("DEVICE.ONLINE", sourceDeviceId, SourceType::FrameField, "", "DeviceStatus"));
    mappings.append(mapping("DEVICE.ERROR_CODE", sourceDeviceId, SourceType::FrameField, "", "ErrorCode"));
    mappings.append(mapping("DEVICE.QUALITY", sourceDeviceId, SourceType::FrameField, "", "Quality"));
    mappings.append(mapping("DEVICE.SEQUENCE_NO", sourceDeviceId, SourceType::FrameField, "", "SequenceNo"));
    mappings.append(mapping("DEVICE.FRAME_INTERVAL_MS", sourceDeviceId, SourceType::Runtime, "", "TimestampDelta"));
    mappings.append(mapping("DEVICE.FRAME_LOSS_COUNT", sourceDeviceId, SourceType::Runtime, "", "SequenceNoDelta"));

    mappings.append(mapping("MEAS.TEMP.CH01", sourceDeviceId, SourceType::Channel, "TEMP_CH01"));
    mappings.append(mapping("MEAS.PRESSURE.CH01", sourceDeviceId, SourceType::Channel, "PRESSURE_CH01"));
    mappings.append(mapping("MEAS.LIGHT.CH01", sourceDeviceId, SourceType::Channel, "LIGHT_CH01"));
    mappings.append(mapping("MEAS.VOLTAGE.CH01", sourceDeviceId, SourceType::Channel, "VOLTAGE_CH01"));
    mappings.append(mapping("MEAS.CURRENT.CH01", sourceDeviceId, SourceType::Channel, "CURRENT_CH01"));
    mappings.append(mapping("MEAS.VIBRATION.CH01", sourceDeviceId, SourceType::Channel, "VIBRATION_CH01"));

    mappings.append(mapping("MEAS.POWER.CH01", sourceDeviceId, SourceType::Derived, "", "", "MEAS.VOLTAGE.CH01 * MEAS.CURRENT.CH01", "MEAS.VOLTAGE.CH01,MEAS.CURRENT.CH01"));
    mappings.append(mapping("MEAS.LOAD_RATIO.CH01", sourceDeviceId, SourceType::Derived, "", "", "MEAS.CURRENT.CH01 / 5.0 * 100", "MEAS.CURRENT.CH01"));

    mappings.append(mapping("MATRIX.LIGHT.AVG", sourceDeviceId, SourceType::Matrix, "", "Average(MatrixValues)"));
    mappings.append(mapping("MATRIX.LIGHT.MAX", sourceDeviceId, SourceType::Matrix, "", "Max(MatrixValues)"));
    mappings.append(mapping("MATRIX.LIGHT.MIN", sourceDeviceId, SourceType::Matrix, "", "Min(MatrixValues)"));
    mappings.append(mapping("MATRIX.LIGHT.UNIFORMITY", sourceDeviceId, SourceType::Matrix, "", "Uniformity(MatrixValues)"));
    mappings.append(mapping("MATRIX.LIGHT.ABNORMAL_COUNT", sourceDeviceId, SourceType::Matrix, "", "Count(MatrixValues outside warning range)"));
    mappings.append(mapping("MATRIX.LIGHT.HOTSPOT_ROW", sourceDeviceId, SourceType::Matrix, "", "ArgMax.Row"));
    mappings.append(mapping("MATRIX.LIGHT.HOTSPOT_COL", sourceDeviceId, SourceType::Matrix, "", "ArgMax.Col"));

    return mappings;
}

} // namespace Monitor::Application::Services
