#include "AlarmService.h"

#include "application/services/TagDefinitionCatalog.h"
#include "domain/common/DomainCommon.h"

#include <QMutexLocker>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace Monitor::Application::Services {
namespace {

constexpr qint64 ValueUpdateIntervalMs = 5000;
constexpr double MinimumAbsoluteValueChange = 1.0;
constexpr double MinimumRelativeValueChange = 0.05;

QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> defaultConfigurations(
    const QVector<Monitor::Domain::Tags::TagDefinition> &definitions)
{
    QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> configurations;
    configurations.reserve(definitions.size());
    for (const auto &definition : definitions) {
        configurations.append(Monitor::Application::Configuration::TagRuntimeConfiguration::fromDefinition(definition));
    }

    return configurations;
}

std::optional<double> numericEquivalent(const Monitor::Domain::Tags::CleanedTagValue &value)
{
    if (value.numericValue.has_value()) {
        return value.numericValue;
    }

    if (value.boolValue.has_value()) {
        return value.boolValue.value() ? 1.0 : 0.0;
    }

    return std::nullopt;
}

} // namespace

AlarmService::AlarmService()
    : AlarmService(TagDefinitionCatalog::createDefaults())
{
}

AlarmService::AlarmService(const QVector<Monitor::Domain::Tags::TagDefinition> &definitions)
    : AlarmService(definitions, defaultConfigurations(definitions))
{
}

AlarmService::AlarmService(
    const QVector<Monitor::Domain::Tags::TagDefinition> &definitions,
    const QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> &configurations)
    : m_configurations(createConfigurationSnapshot(configurations))
{
    for (const auto &definition : definitions) {
        m_definitions.insert(definition.tagId, definition);
    }
}

QVector<Monitor::Domain::Alarms::AlarmEvent> AlarmService::activeAlarms() const
{
    return currentAlarms();
}

QVector<Monitor::Domain::Alarms::AlarmEvent> AlarmService::currentAlarms() const
{
    QMutexLocker locker(&m_mutex);
    return currentAlarmsLocked();
}

QVector<Monitor::Domain::Alarms::AlarmEvent> AlarmService::recentAlarmEvents(int count) const
{
    if (count <= 0) {
        return {};
    }

    auto events = alarmEvents();
    if (events.size() <= count) {
        return events;
    }

    events.resize(count);
    return events;
}

QVector<Monitor::Domain::Alarms::AlarmEvent> AlarmService::alarmEvents() const
{
    QMutexLocker locker(&m_mutex);

    QVector<Monitor::Domain::Alarms::AlarmEvent> events;
    events.reserve(m_alarmEvents.size());
    for (auto it = m_alarmEvents.cbegin(); it != m_alarmEvents.cend(); ++it) {
        events.append(it.value());
    }

    std::sort(events.begin(), events.end(), [](const auto &left, const auto &right) {
        return left.triggerTimeUtc > right.triggerTimeUtc;
    });

    return events;
}

QVector<Monitor::Domain::Tags::TagRuntimeState> AlarmService::evaluate(
    const QVector<Monitor::Domain::Tags::CleanedTagValue> &values,
    const QDateTime &lastUpdateTimeUtc)
{
    return evaluateWithChanges(values, lastUpdateTimeUtc).states;
}

Monitor::Application::Dtos::AlarmEvaluationResult AlarmService::evaluateWithChanges(
    const QVector<Monitor::Domain::Tags::CleanedTagValue> &values,
    const QDateTime &lastUpdateTimeUtc)
{
    using Monitor::Domain::Common::UtcDateTime;
    using Monitor::Domain::Tags::TagCategory;
    using Monitor::Domain::Tags::TagDataType;
    using Monitor::Domain::Tags::TagRuntimeState;

    UtcDateTime::require(lastUpdateTimeUtc, QStringLiteral("lastUpdateTimeUtc"));
    for (const auto &value : values) {
        UtcDateTime::require(value.timestampUtc, QStringLiteral("values.timestampUtc"));
    }

    QHash<QString, Monitor::Application::Configuration::TagRuntimeConfiguration> configurations;
    {
        QMutexLocker locker(&m_mutex);
        configurations = m_configurations;
    }

    QVector<TagRuntimeState> states;
    states.reserve(values.size());
    for (const auto &value : values) {
        const auto definitionIt = m_definitions.constFind(value.tagId);
        const auto configurationIt = configurations.constFind(value.tagId);
        const auto configuration = configurationIt == configurations.cend()
            ? std::optional<Monitor::Application::Configuration::TagRuntimeConfiguration>()
            : std::optional<Monitor::Application::Configuration::TagRuntimeConfiguration>(configurationIt.value());
        const auto numericValue = numericEquivalent(value);

        states.append(TagRuntimeState{
            value.tagId,
            definitionIt == m_definitions.cend() ? value.tagId : definitionIt.value().displayName,
            definitionIt == m_definitions.cend() ? TagCategory::Runtime : definitionIt.value().category,
            value.numericValue,
            value.textValue,
            value.boolValue,
            definitionIt == m_definitions.cend() ? value.unit : std::optional<QString>(definitionIt.value().unit),
            definitionIt == m_definitions.cend() ? value.dataType : definitionIt.value().dataType,
            value.quality,
            evaluateAlarmState(numericValue, value.quality, configuration),
            value.timestampUtc,
            value.sourceFrameId,
            value.sequenceNo,
            lastUpdateTimeUtc
        });
    }

    QMutexLocker locker(&m_mutex);
    return {states, updateActiveAlarms(states, configurations)};
}

bool AlarmService::acknowledge(
    const QUuid &alarmId,
    const QDateTime &acknowledgedAtUtc,
    Monitor::Domain::Alarms::AlarmEvent *acknowledgedAlarm)
{
    using Monitor::Domain::Alarms::AlarmState;

    Monitor::Domain::Common::UtcDateTime::require(acknowledgedAtUtc, QStringLiteral("acknowledgedAtUtc"));

    QMutexLocker locker(&m_mutex);
    const auto alarmIt = m_alarmEvents.find(alarmId);
    if (alarmIt == m_alarmEvents.end() ||
        !m_currentAlarmIds.values().contains(alarmId) ||
        alarmIt.value().state != AlarmState::Active) {
        if (acknowledgedAlarm) {
            *acknowledgedAlarm = {};
        }
        return false;
    }

    alarmIt.value().state = AlarmState::Acknowledged;
    alarmIt.value().acknowledgeTimeUtc = acknowledgedAtUtc;
    alarmIt.value().lastUpdatedTimeUtc = acknowledgedAtUtc;

    if (acknowledgedAlarm) {
        *acknowledgedAlarm = alarmIt.value();
    }
    return true;
}

void AlarmService::restoreEvents(const QVector<Monitor::Domain::Alarms::AlarmEvent> &alarms)
{
    using Monitor::Domain::Alarms::AlarmState;
    using Monitor::Domain::Common::UtcDateTime;

    QMutexLocker locker(&m_mutex);
    QVector<Monitor::Domain::Alarms::AlarmEvent> ordered = alarms;
    std::sort(ordered.begin(), ordered.end(), [](const auto &left, const auto &right) {
        return left.triggerTimeUtc > right.triggerTimeUtc;
    });

    for (const auto &alarm : ordered) {
        UtcDateTime::require(alarm.triggerTimeUtc, QStringLiteral("alarms.triggerTimeUtc"));
        UtcDateTime::require(alarm.acknowledgeTimeUtc, QStringLiteral("alarms.acknowledgeTimeUtc"));
        UtcDateTime::require(alarm.recoverTimeUtc, QStringLiteral("alarms.recoverTimeUtc"));
        m_alarmEvents.insert(alarm.alarmId, alarm);
        if (alarm.state == AlarmState::Active || alarm.state == AlarmState::Acknowledged) {
            m_currentAlarmIds.insert(alarm.tagId, alarm.alarmId);
        }
    }
}

void AlarmService::replaceConfigurations(
    const QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> &configurations)
{
    QMutexLocker locker(&m_mutex);
    m_configurations = createConfigurationSnapshot(configurations);
}

QVector<Monitor::Domain::Alarms::AlarmEvent> AlarmService::currentAlarmsLocked() const
{
    using Monitor::Domain::Alarms::AlarmState;

    QVector<Monitor::Domain::Alarms::AlarmEvent> result;
    result.reserve(m_currentAlarmIds.size());

    for (auto it = m_currentAlarmIds.cbegin(); it != m_currentAlarmIds.cend(); ++it) {
        const auto alarmIt = m_alarmEvents.constFind(it.value());
        if (alarmIt == m_alarmEvents.cend()) {
            continue;
        }

        if (alarmIt.value().state == AlarmState::Active ||
            alarmIt.value().state == AlarmState::Acknowledged) {
            result.append(alarmIt.value());
        }
    }

    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        return left.triggerTimeUtc > right.triggerTimeUtc;
    });

    return result;
}

QVector<Monitor::Application::Dtos::AlarmLifecycleChange> AlarmService::updateActiveAlarms(
    const QVector<Monitor::Domain::Tags::TagRuntimeState> &values,
    const QHash<QString, Monitor::Application::Configuration::TagRuntimeConfiguration> &configurations)
{
    using Monitor::Application::Dtos::AlarmLifecycleChange;
    using Monitor::Application::Dtos::AlarmLifecycleChangeType;
    using Monitor::Domain::Alarms::AlarmEvent;
    using Monitor::Domain::Alarms::AlarmState;
    using Monitor::Domain::Tags::TagAlarmState;

    QVector<AlarmLifecycleChange> lifecycleChanges;
    for (const auto &value : values) {
        const auto configurationIt = configurations.constFind(value.tagId);
        const auto configurationExists = configurationIt != configurations.cend();
        const auto configurationChanged = configurationExists
            && m_evaluatedConfigurationRevisions.contains(value.tagId)
            && m_evaluatedConfigurationRevisions.value(value.tagId) != configurationIt.value().revision;
        if (configurationExists) {
            m_evaluatedConfigurationRevisions.insert(value.tagId, configurationIt.value().revision);
        }

        if (value.alarmState == TagAlarmState::Normal) {
            const auto reason = configurationChanged ||
                    (configurationExists && !configurationIt.value().alarmEnabled)
                ? QStringLiteral("ConfigurationChanged")
                : QStringLiteral("ValueReturnedToNormal");
            const auto recoveredAlarm = recover(value, reason);
            if (recoveredAlarm.has_value()) {
                lifecycleChanges.append(AlarmLifecycleChange{AlarmLifecycleChangeType::Recovered, recoveredAlarm.value()});
            }
            continue;
        }

        const auto level = alarmLevel(value.alarmState);
        const auto newTriggerValue = triggerValue(value);
        const auto message = QStringLiteral("%1 %2: %3")
            .arg(value.tagId, Monitor::Domain::Tags::toString(value.alarmState), formatTriggerValue(value));

        const auto currentAlarmIt = m_currentAlarmIds.constFind(value.tagId);
        if (currentAlarmIt != m_currentAlarmIds.cend()) {
            const auto eventIt = m_alarmEvents.find(currentAlarmIt.value());
            if (eventIt != m_alarmEvents.end()) {
                const auto shouldPublishUpdate = eventIt.value().level != level ||
                    eventIt.value().alarmType != value.alarmState ||
                    shouldPublishValueUpdate(eventIt.value(), newTriggerValue, value.timestampUtc);
                if (!shouldPublishUpdate) {
                    continue;
                }

                eventIt.value().level = level;
                eventIt.value().alarmType = value.alarmState;
                eventIt.value().triggerValue = newTriggerValue;
                eventIt.value().message = message;
                eventIt.value().lastUpdatedTimeUtc = value.timestampUtc;
                lifecycleChanges.append(AlarmLifecycleChange{AlarmLifecycleChangeType::Updated, eventIt.value()});
                continue;
            }
        }

        AlarmEvent alarm;
        alarm.alarmId = QUuid::createUuid();
        alarm.tagId = value.tagId;
        alarm.level = level;
        alarm.state = AlarmState::Active;
        alarm.triggerValue = newTriggerValue;
        alarm.triggerTimeUtc = value.timestampUtc;
        alarm.message = message;
        alarm.alarmType = value.alarmState;
        alarm.lastUpdatedTimeUtc = value.timestampUtc;
        m_alarmEvents.insert(alarm.alarmId, alarm);
        m_currentAlarmIds.insert(value.tagId, alarm.alarmId);
        lifecycleChanges.append(AlarmLifecycleChange{AlarmLifecycleChangeType::Raised, alarm});
    }

    return lifecycleChanges;
}

std::optional<Monitor::Domain::Alarms::AlarmEvent> AlarmService::recover(
    const Monitor::Domain::Tags::TagRuntimeState &value,
    const QString &reason)
{
    using Monitor::Domain::Alarms::AlarmState;

    const auto currentAlarmIt = m_currentAlarmIds.find(value.tagId);
    if (currentAlarmIt == m_currentAlarmIds.end()) {
        return std::nullopt;
    }

    const auto alarmIt = m_alarmEvents.find(currentAlarmIt.value());
    if (alarmIt == m_alarmEvents.end()) {
        m_currentAlarmIds.erase(currentAlarmIt);
        return std::nullopt;
    }

    alarmIt.value().state = AlarmState::Recovered;
    if (!alarmIt.value().recoverTimeUtc.has_value()) {
        alarmIt.value().recoverTimeUtc = value.timestampUtc;
    }
    alarmIt.value().message = QStringLiteral("%1 recovered: %2").arg(value.tagId, reason);
    alarmIt.value().lastUpdatedTimeUtc = value.timestampUtc;
    alarmIt.value().closeReason = reason;

    const auto recoveredAlarm = alarmIt.value();
    m_currentAlarmIds.erase(currentAlarmIt);
    return recoveredAlarm;
}

QHash<QString, Monitor::Application::Configuration::TagRuntimeConfiguration> AlarmService::createConfigurationSnapshot(
    const QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> &configurations)
{
    QHash<QString, Monitor::Application::Configuration::TagRuntimeConfiguration> snapshot;
    for (const auto &configuration : configurations) {
        snapshot.insert(configuration.tagId, configuration);
    }

    return snapshot;
}

Monitor::Domain::Tags::TagAlarmState AlarmService::evaluateAlarmState(
    const std::optional<double> &value,
    Monitor::Domain::Tags::TagQuality quality,
    const std::optional<Monitor::Application::Configuration::TagRuntimeConfiguration> &configuration)
{
    using Monitor::Domain::Tags::TagAlarmState;
    using Monitor::Domain::Tags::TagQuality;

    if (configuration.has_value() && !configuration->alarmEnabled) {
        return TagAlarmState::Normal;
    }

    if (quality == TagQuality::Offline) {
        return TagAlarmState::Offline;
    }

    if (quality != TagQuality::Good) {
        return TagAlarmState::Invalid;
    }

    if (!configuration.has_value() || !value.has_value()) {
        return TagAlarmState::Normal;
    }

    if (configuration->alarmHigh.has_value() && value.value() >= configuration->alarmHigh.value()) {
        return TagAlarmState::AlarmHigh;
    }

    if (configuration->alarmLow.has_value() && value.value() <= configuration->alarmLow.value()) {
        return TagAlarmState::AlarmLow;
    }

    if (configuration->warningHigh.has_value() && value.value() >= configuration->warningHigh.value()) {
        return TagAlarmState::WarningHigh;
    }

    if (configuration->warningLow.has_value() && value.value() <= configuration->warningLow.value()) {
        return TagAlarmState::WarningLow;
    }

    return TagAlarmState::Normal;
}

Monitor::Domain::Alarms::AlarmLevel AlarmService::alarmLevel(
    Monitor::Domain::Tags::TagAlarmState alarmState)
{
    using Monitor::Domain::Alarms::AlarmLevel;
    using Monitor::Domain::Tags::TagAlarmState;

    if (alarmState == TagAlarmState::WarningHigh || alarmState == TagAlarmState::WarningLow) {
        return AlarmLevel::Warning;
    }

    if (alarmState == TagAlarmState::Invalid || alarmState == TagAlarmState::Offline) {
        return AlarmLevel::Quality;
    }

    return AlarmLevel::Alarm;
}

double AlarmService::triggerValue(const Monitor::Domain::Tags::TagRuntimeState &value)
{
    if (value.numericValue.has_value()) {
        return value.numericValue.value();
    }

    if (value.boolValue.has_value()) {
        return value.boolValue.value() ? 1.0 : 0.0;
    }

    return 0.0;
}

QString AlarmService::formatTriggerValue(const Monitor::Domain::Tags::TagRuntimeState &value)
{
    if (value.numericValue.has_value()) {
        return QString::number(value.numericValue.value(), 'f', 3)
            .remove(QRegularExpression(QStringLiteral("\\.?0+$")));
    }

    if (value.boolValue.has_value()) {
        return value.boolValue.value() ? QStringLiteral("True") : QStringLiteral("False");
    }

    return value.textValue.value_or(QString());
}

bool AlarmService::shouldPublishValueUpdate(
    const Monitor::Domain::Alarms::AlarmEvent &existingAlarm,
    double newTriggerValue,
    const QDateTime &timestampUtc)
{
    const auto lastUpdatedUtc = existingAlarm.lastUpdatedTimeUtc.value_or(existingAlarm.triggerTimeUtc);
    if (lastUpdatedUtc.msecsTo(timestampUtc) >= ValueUpdateIntervalMs) {
        return true;
    }

    const auto absoluteChange = std::abs(newTriggerValue - existingAlarm.triggerValue);
    const auto relativeBaseline = std::max(std::abs(existingAlarm.triggerValue), 1.0);
    return absoluteChange >= MinimumAbsoluteValueChange &&
        absoluteChange / relativeBaseline >= MinimumRelativeValueChange;
}

} // namespace Monitor::Application::Services
