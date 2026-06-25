#include "ConfigurationValidation.h"

#include <cmath>
#include <stdexcept>

namespace Monitor::Application::Configuration {
namespace {

void throwArgument(const QString &message)
{
    throw std::invalid_argument(message.toStdString());
}

bool hasThresholds(const TagRuntimeConfiguration &configuration)
{
    return configuration.alarmLow.has_value()
        || configuration.warningLow.has_value()
        || configuration.warningHigh.has_value()
        || configuration.alarmHigh.has_value();
}

} // namespace

void ConfigurationValidation::validateTag(
    const Monitor::Domain::Tags::TagDefinition &definition,
    const TagRuntimeConfiguration &configuration)
{
    using Monitor::Domain::Tags::isNumeric;

    if (definition.tagId != configuration.tagId) {
        throwArgument(QStringLiteral("Tag configuration does not match its definition."));
    }

    const QVector<std::optional<double>> thresholds = {
        configuration.alarmLow,
        configuration.warningLow,
        configuration.warningHigh,
        configuration.alarmHigh
    };

    for (const auto &threshold : thresholds) {
        if (threshold.has_value() && !std::isfinite(threshold.value())) {
            throwArgument(QStringLiteral("%1: thresholds must be finite numbers.").arg(definition.tagId));
        }
    }

    const auto numeric = isNumeric(definition.dataType);
    if (!numeric && hasThresholds(configuration)) {
        throwArgument(QStringLiteral("%1: non-numeric tags cannot define numeric thresholds.").arg(definition.tagId));
    }

    validateBound(definition.tagId, QStringLiteral("AlarmLow"), configuration.alarmLow, definition.minValue, definition.maxValue);
    validateBound(definition.tagId, QStringLiteral("WarningLow"), configuration.warningLow, definition.minValue, definition.maxValue);
    validateBound(definition.tagId, QStringLiteral("WarningHigh"), configuration.warningHigh, definition.minValue, definition.maxValue);
    validateBound(definition.tagId, QStringLiteral("AlarmHigh"), configuration.alarmHigh, definition.minValue, definition.maxValue);

    if (configuration.alarmLow.has_value() &&
        configuration.warningLow.has_value() &&
        configuration.alarmLow.value() > configuration.warningLow.value()) {
        throwArgument(QStringLiteral("%1: AlarmLow must be less than or equal to WarningLow.").arg(definition.tagId));
    }

    if (configuration.warningLow.has_value() &&
        configuration.warningHigh.has_value() &&
        configuration.warningLow.value() >= configuration.warningHigh.value()) {
        throwArgument(QStringLiteral("%1: WarningLow must be less than WarningHigh.").arg(definition.tagId));
    }

    if (configuration.warningHigh.has_value() &&
        configuration.alarmHigh.has_value() &&
        configuration.warningHigh.value() > configuration.alarmHigh.value()) {
        throwArgument(QStringLiteral("%1: WarningHigh must be less than or equal to AlarmHigh.").arg(definition.tagId));
    }

    if (configuration.alarmEnabled && numeric && !hasThresholds(configuration)) {
        throwArgument(QStringLiteral("%1: enabled numeric alarms require at least one threshold.").arg(definition.tagId));
    }

    if (configuration.historyIntervalMs <= 0) {
        throwArgument(QStringLiteral("%1: HistoryIntervalMs must be greater than zero.").arg(definition.tagId));
    }
}

void ConfigurationValidation::validateRuntimeOptions(const MonitorRuntimeOptions &options)
{
    if (options.dataGenerateIntervalMs <= 0 ||
        options.uiRefreshIntervalMs <= 0 ||
        options.historyBatchIntervalMs <= 0 ||
        options.alarmBatchIntervalMs <= 0 ||
        options.operationLogBatchIntervalMs <= 0) {
        throwArgument(QStringLiteral("All runtime intervals must be greater than zero."));
    }

    if (options.trendWindowMinutes.isEmpty()) {
        throwArgument(QStringLiteral("At least one positive trend window is required."));
    }

    for (const auto window : options.trendWindowMinutes) {
        if (window <= 0) {
            throwArgument(QStringLiteral("At least one positive trend window is required."));
        }
    }

    if (options.historyRetentionDays <= 0 || options.historyRetentionDeleteBatchSize <= 0) {
        throwArgument(QStringLiteral("History retention days and delete batch size must be greater than zero."));
    }

    if (options.dataSourceTimeoutPeriods < 2) {
        throwArgument(QStringLiteral("Data source timeout periods must be at least 2."));
    }
}

void ConfigurationValidation::validateBound(
    const QString &tagId,
    const QString &name,
    const std::optional<double> &value,
    const std::optional<double> &minimum,
    const std::optional<double> &maximum)
{
    if (value.has_value() && minimum.has_value() && value.value() < minimum.value()) {
        throwArgument(QStringLiteral("%1: %2 is below MinValue.").arg(tagId, name));
    }

    if (value.has_value() && maximum.has_value() && value.value() > maximum.value()) {
        throwArgument(QStringLiteral("%1: %2 is above MaxValue.").arg(tagId, name));
    }
}

} // namespace Monitor::Application::Configuration
