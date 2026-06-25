#ifndef DASHBOARDSERVICE_H
#define DASHBOARDSERVICE_H

#include "application/dto/ApplicationDtos.h"
#include "application/services/AlarmService.h"
#include "application/services/TagService.h"

namespace Monitor::Application::Services {

class DashboardService
{
public:
    DashboardService(const TagService *tagService = nullptr, const AlarmService *alarmService = nullptr);

    Monitor::Application::Dtos::DashboardSnapshot snapshot(const QDateTime &capturedAtUtc) const;
    Monitor::Application::Dtos::DashboardSnapshot buildSnapshot(
        const Monitor::Domain::Tags::TagSnapshot &tagSnapshot,
        const QVector<Monitor::Domain::Alarms::AlarmEvent> &currentAlarms,
        const QDateTime &capturedAtUtc) const;
    Monitor::Application::Dtos::DashboardSnapshot buildSnapshot(
        const QVector<Monitor::Domain::Tags::TagRuntimeState> &currentValues,
        const QVector<Monitor::Domain::Alarms::AlarmEvent> &currentAlarms,
        const QDateTime &capturedAtUtc) const;

private:
    const TagService *m_tagService = nullptr;
    const AlarmService *m_alarmService = nullptr;
};

} // namespace Monitor::Application::Services

#endif // DASHBOARDSERVICE_H
