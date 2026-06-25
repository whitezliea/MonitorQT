#ifndef ALARMSERVICE_H
#define ALARMSERVICE_H

#include "application/configuration/TagRuntimeConfiguration.h"
#include "application/dto/ApplicationDtos.h"
#include "domain/alarms/AlarmModels.h"
#include "domain/tags/TagModels.h"

#include <QHash>
#include <QMutex>
#include <QString>
#include <QUuid>
#include <QVector>

#include <optional>

namespace Monitor::Application::Services {

class AlarmService
{
public:
    AlarmService();
    explicit AlarmService(const QVector<Monitor::Domain::Tags::TagDefinition> &definitions);
    AlarmService(
        const QVector<Monitor::Domain::Tags::TagDefinition> &definitions,
        const QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> &configurations);

    QVector<Monitor::Domain::Alarms::AlarmEvent> activeAlarms() const;
    QVector<Monitor::Domain::Alarms::AlarmEvent> currentAlarms() const;
    QVector<Monitor::Domain::Alarms::AlarmEvent> recentAlarmEvents(int count = 100) const;
    QVector<Monitor::Domain::Alarms::AlarmEvent> alarmEvents() const;

    QVector<Monitor::Domain::Tags::TagRuntimeState> evaluate(
        const QVector<Monitor::Domain::Tags::CleanedTagValue> &values,
        const QDateTime &lastUpdateTimeUtc);
    Monitor::Application::Dtos::AlarmEvaluationResult evaluateWithChanges(
        const QVector<Monitor::Domain::Tags::CleanedTagValue> &values,
        const QDateTime &lastUpdateTimeUtc);

    bool acknowledge(
        const QUuid &alarmId,
        const QDateTime &acknowledgedAtUtc,
        Monitor::Domain::Alarms::AlarmEvent *acknowledgedAlarm = nullptr);
    void restoreEvents(const QVector<Monitor::Domain::Alarms::AlarmEvent> &alarms);
    void replaceConfigurations(
        const QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> &configurations);

private:
    QVector<Monitor::Domain::Alarms::AlarmEvent> currentAlarmsLocked() const;
    QVector<Monitor::Application::Dtos::AlarmLifecycleChange> updateActiveAlarms(
        const QVector<Monitor::Domain::Tags::TagRuntimeState> &values,
        const QHash<QString, Monitor::Application::Configuration::TagRuntimeConfiguration> &configurations);
    std::optional<Monitor::Domain::Alarms::AlarmEvent> recover(
        const Monitor::Domain::Tags::TagRuntimeState &value,
        const QString &reason);

    static QHash<QString, Monitor::Application::Configuration::TagRuntimeConfiguration> createConfigurationSnapshot(
        const QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> &configurations);
    static Monitor::Domain::Tags::TagAlarmState evaluateAlarmState(
        const std::optional<double> &value,
        Monitor::Domain::Tags::TagQuality quality,
        const std::optional<Monitor::Application::Configuration::TagRuntimeConfiguration> &configuration);
    static Monitor::Domain::Alarms::AlarmLevel alarmLevel(
        Monitor::Domain::Tags::TagAlarmState alarmState);
    static double triggerValue(const Monitor::Domain::Tags::TagRuntimeState &value);
    static QString formatTriggerValue(const Monitor::Domain::Tags::TagRuntimeState &value);
    static bool shouldPublishValueUpdate(
        const Monitor::Domain::Alarms::AlarmEvent &existingAlarm,
        double newTriggerValue,
        const QDateTime &timestampUtc);

    QHash<QString, Monitor::Domain::Tags::TagDefinition> m_definitions;
    QHash<QString, Monitor::Application::Configuration::TagRuntimeConfiguration> m_configurations;
    QHash<QString, QUuid> m_currentAlarmIds;
    QHash<QUuid, Monitor::Domain::Alarms::AlarmEvent> m_alarmEvents;
    QHash<QString, qint64> m_evaluatedConfigurationRevisions;
    mutable QMutex m_mutex;
};

} // namespace Monitor::Application::Services

#endif // ALARMSERVICE_H
