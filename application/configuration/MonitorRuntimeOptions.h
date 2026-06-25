#ifndef MONITORRUNTIMEOPTIONS_H
#define MONITORRUNTIMEOPTIONS_H

#include <QHash>
#include <QString>
#include <QVector>

namespace Monitor::Application::Configuration {

enum class SettingEffect {
    Immediate,
    NextAcquisitionStart,
    NextApplicationStart
};

struct RuntimeSettingKeys
{
    static const QString DataGenerateIntervalMs;
    static const QString DataSourceTimeoutPeriods;
    static const QString UiRefreshIntervalMs;
    static const QString HistoryBatchIntervalMs;
    static const QString HistoryRetentionDays;
    static const QString AlarmBatchIntervalMs;
    static const QString OperationLogBatchIntervalMs;
    static const QString MaximumTrendWindowMinutes;
};

struct MonitorRuntimeOptions
{
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

    int maximumTrendWindowMinutes() const;
    int trendPointCount(int windowMinutes) const;
    int trendBufferCapacity() const;
    int dataSourceTimeoutMs() const;
};

struct RuntimeSettingsSaveResult
{
    MonitorRuntimeOptions options;
    QHash<QString, SettingEffect> effects;
};

QString toString(SettingEffect effect);

} // namespace Monitor::Application::Configuration

#endif // MONITORRUNTIMEOPTIONS_H
