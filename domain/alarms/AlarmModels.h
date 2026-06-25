#ifndef ALARMMODELS_H
#define ALARMMODELS_H

#include "domain/tags/TagModels.h"

#include <QDateTime>
#include <QString>
#include <QUuid>

#include <optional>

namespace Monitor::Domain::Alarms {

enum class AlarmLevel {
    Warning,
    Alarm,
    Critical,
    Quality
};

enum class AlarmState {
    Active,
    Acknowledged,
    Recovered
};

struct AlarmDefinition
{
    QString tagId;
    std::optional<double> warningHigh;
    std::optional<double> alarmHigh;
    std::optional<double> warningLow;
    std::optional<double> alarmLow;
    double hysteresis = 0.0;
    int debounceCount = 1;

    static AlarmDefinition fromTagDefinition(const Tags::TagDefinition &definition);
};

struct AlarmEvent
{
    QUuid alarmId;
    QString tagId;
    AlarmLevel level = AlarmLevel::Warning;
    AlarmState state = AlarmState::Active;
    double triggerValue = 0.0;
    QDateTime triggerTimeUtc;
    QString message;
    std::optional<QDateTime> acknowledgeTimeUtc;
    std::optional<QDateTime> recoverTimeUtc;
    Tags::TagAlarmState alarmType = Tags::TagAlarmState::Invalid;
    std::optional<QDateTime> lastUpdatedTimeUtc;
    std::optional<QString> closeReason;
};

struct ActiveAlarm
{
    AlarmEvent event;
    bool isAcknowledged = false;
};

QString toString(AlarmLevel level);
QString toString(AlarmState state);

} // namespace Monitor::Domain::Alarms

#endif // ALARMMODELS_H
