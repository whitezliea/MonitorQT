#include "MonitorRuntimeOptions.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Monitor::Application::Configuration {

const QString RuntimeSettingKeys::DataGenerateIntervalMs = QStringLiteral("DataGenerateIntervalMs");
const QString RuntimeSettingKeys::DataSourceTimeoutPeriods = QStringLiteral("DataSourceTimeoutPeriods");
const QString RuntimeSettingKeys::UiRefreshIntervalMs = QStringLiteral("UiRefreshIntervalMs");
const QString RuntimeSettingKeys::HistoryBatchIntervalMs = QStringLiteral("HistoryBatchIntervalMs");
const QString RuntimeSettingKeys::HistoryRetentionDays = QStringLiteral("HistoryRetentionDays");
const QString RuntimeSettingKeys::AlarmBatchIntervalMs = QStringLiteral("AlarmBatchIntervalMs");
const QString RuntimeSettingKeys::OperationLogBatchIntervalMs = QStringLiteral("OperationLogBatchIntervalMs");
const QString RuntimeSettingKeys::MaximumTrendWindowMinutes = QStringLiteral("MaximumTrendWindowMinutes");

int MonitorRuntimeOptions::maximumTrendWindowMinutes() const
{
    if (trendWindowMinutes.isEmpty()) {
        return 0;
    }

    return *std::max_element(trendWindowMinutes.cbegin(), trendWindowMinutes.cend());
}

int MonitorRuntimeOptions::trendPointCount(int windowMinutes) const
{
    if (dataGenerateIntervalMs <= 0) {
        throw std::invalid_argument("Data generation interval must be greater than zero.");
    }

    if (windowMinutes <= 0) {
        throw std::invalid_argument("Trend window must be greater than zero.");
    }

    return static_cast<int>(std::ceil(windowMinutes * 60'000.0 / dataGenerateIntervalMs));
}

int MonitorRuntimeOptions::trendBufferCapacity() const
{
    const auto maximumWindow = maximumTrendWindowMinutes();
    return maximumWindow <= 0 ? 0 : trendPointCount(maximumWindow);
}

int MonitorRuntimeOptions::dataSourceTimeoutMs() const
{
    return dataGenerateIntervalMs * dataSourceTimeoutPeriods;
}

QString toString(SettingEffect effect)
{
    switch (effect) {
    case SettingEffect::Immediate:
        return QStringLiteral("Immediate");
    case SettingEffect::NextAcquisitionStart:
        return QStringLiteral("NextAcquisitionStart");
    case SettingEffect::NextApplicationStart:
        return QStringLiteral("NextApplicationStart");
    }

    return QString();
}

} // namespace Monitor::Application::Configuration
