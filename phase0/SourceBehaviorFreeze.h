#ifndef SOURCEBEHAVIORFREEZE_H
#define SOURCEBEHAVIORFREEZE_H

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVector>

#include <optional>

namespace Phase0 {

enum class DeviceStatus {
    Stopped,
    Running,
    Warning,
    Error,
    Offline
};

enum class TagCategory {
    Measurement,
    Mechanical,
    Runtime,
    Temperature,
    Pressure,
    Light,
    Electrical,
    Vibration,
    Device,
    Matrix,
    Derived
};

enum class TagDataType {
    Double,
    Int,
    String,
    Enum,
    Number,
    Boolean,
    Text
};

enum class TagValueKind {
    Numeric,
    Boolean,
    Text,
    Enum,
    MatrixStat,
    DerivedNumeric
};

enum class TagQuality {
    Good,
    Bad,
    Timeout,
    OutOfRange,
    DeviceError,
    Offline
};

enum class TagAlarmState {
    Normal,
    WarningHigh,
    WarningLow,
    AlarmHigh,
    AlarmLow,
    Invalid,
    Offline
};

enum class SourceType {
    FrameField,
    Channel,
    Matrix,
    Derived,
    Runtime
};

struct RuntimeOptions {
    int dataGenerateIntervalMs = 500;
    int dataSourceTimeoutPeriods = 3;
    int uiRefreshIntervalMs = 1000;
    int historyBatchIntervalMs = 5000;
    int historyMaxBatchSize = 100;
    int historyRetentionDays = 30;
    int historyRetentionDeleteBatchSize = 1000;
    int alarmBatchIntervalMs = 5000;
    int alarmMaxBatchSize = 100;
    int operationLogBatchIntervalMs = 5000;
    int operationLogMaxBatchSize = 100;
    QVector<int> trendWindowMinutes = {1, 5, 30};

    int dataSourceTimeoutMs() const;
    int trendPointCount(int windowMinutes) const;
    int trendBufferCapacity() const;
};

struct TagDefinition {
    QString tagId;
    QString displayName;
    TagCategory category = TagCategory::Runtime;
    QString unit;
    std::optional<double> minValue;
    std::optional<double> maxValue;
    std::optional<double> warningHigh;
    std::optional<double> alarmHigh;
    std::optional<double> warningLow;
    std::optional<double> alarmLow;
    bool isEnabled = true;
    QString description;
    TagDataType dataType = TagDataType::Double;
    TagValueKind valueKind = TagValueKind::Numeric;
    bool isHistorized = true;
    std::optional<int> historyIntervalMs = 1000;
    int displayOrder = 0;
};

struct TagSourceMapping {
    QString tagId;
    QString sourceDeviceId;
    SourceType sourceType = SourceType::FrameField;
    QString sourceCode;
    QString sourcePath;
    double scale = 1.0;
    double offset = 0.0;
    QString formula;
    QString inputTagIds;
    bool isEnabled = true;
};

struct ChannelValue {
    QString channelId;
    double value = 0.0;
    QString unit;
    TagQuality quality = TagQuality::Good;
    int errorCode = 0;
};

struct MatrixStatistics {
    double minValue = 0.0;
    double maxValue = 0.0;
    double averageValue = 0.0;
    double stdDev = 0.0;
    double uniformityMinMax = 0.0;
    double uniformityMinAverage = 0.0;
    int validCount = 0;
    int invalidCount = 0;

    double uniformity() const;
};

struct MatrixFrame {
    QUuid frameId;
    QDateTime timestampUtc;
    int rows = 0;
    int columns = 0;
    QVector<double> values;
    QUuid sourceFrameId;
    qint64 sequenceNo = 0;

    double valueAt(int row, int column) const;
};

struct RawMeasurementFrame {
    QUuid frameId;
    QString deviceId;
    qint64 sequenceNo = 0;
    QDateTime timestampUtc;
    DeviceStatus deviceStatus = DeviceStatus::Stopped;
    QVector<ChannelValue> channelValues;
    MatrixFrame matrixValues;
    bool hasMatrixValues = false;
    int errorCode = 0;
    TagQuality quality = TagQuality::Good;
};

struct CleanedTagValue {
    QString tagId;
    std::optional<double> numericValue;
    std::optional<QString> textValue;
    std::optional<bool> boolValue;
    TagDataType dataType = TagDataType::Double;
    QString unit;
    QDateTime timestampUtc;
    TagQuality quality = TagQuality::Good;
    TagAlarmState alarmState = TagAlarmState::Normal;
    QString sourceDeviceId;
    QString sourceCode;
    QUuid sourceFrameId;
    qint64 sequenceNo = 0;
    QString cleanMessage;
};

struct AcceptanceEvaluation {
    RawMeasurementFrame frame;
    QVector<CleanedTagValue> cleanedValues;
    MatrixStatistics matrixStatistics;
    int matrixAbnormalPointCount = 0;
    QVector<QString> historySampleCandidateTagIds;
};

struct ModuleMappingItem {
    QString sourceModule;
    QString responsibility;
    QString targetModule;
    QString migrationNote;
};

struct PageFeatureChecklistItem {
    QString pageName;
    QString sourceViewModel;
    QString targetWidget;
    QStringList requiredCapabilities;
};

struct TestReplicationChecklistItem {
    QString sourceArea;
    QString sourcePath;
    int testCount = 0;
    QString qtReplicationPriority;
    QString frozenBehavior;
};

const QString &defaultDeviceId();
RuntimeOptions defaultRuntimeOptions();
QVector<TagDefinition> defaultTagDefinitions();
QVector<TagSourceMapping> defaultSourceMappings();
QVector<ModuleMappingItem> moduleMappingChecklist();
QVector<PageFeatureChecklistItem> pageFeatureChecklist();
QVector<TestReplicationChecklistItem> testReplicationChecklist();

RawMeasurementFrame createMinimumAcceptanceFrame();
AcceptanceEvaluation evaluateMinimumAcceptanceFrame();
AcceptanceEvaluation evaluateFrame(const RawMeasurementFrame &frame);
bool validateSourceBehaviorFreeze(QStringList *errors = nullptr);

QString toString(DeviceStatus status);
QString toString(TagQuality quality);
QString toString(TagAlarmState state);

} // namespace Phase0

#endif // SOURCEBEHAVIORFREEZE_H
