#include "DashboardService.h"

#include "domain/common/DomainCommon.h"

#include <algorithm>
#include <stdexcept>

namespace Monitor::Application::Services {

DashboardService::DashboardService(const TagService *tagService, const AlarmService *alarmService)
    : m_tagService(tagService),
      m_alarmService(alarmService)
{
}

Monitor::Application::Dtos::DashboardSnapshot DashboardService::snapshot(
    const QDateTime &capturedAtUtc) const
{
    if (!m_tagService || !m_alarmService) {
        throw std::logic_error("DashboardService requires TagService and AlarmService for snapshot().");
    }

    return buildSnapshot(m_tagService->snapshot(), m_alarmService->currentAlarms(), capturedAtUtc);
}

Monitor::Application::Dtos::DashboardSnapshot DashboardService::buildSnapshot(
    const Monitor::Domain::Tags::TagSnapshot &tagSnapshot,
    const QVector<Monitor::Domain::Alarms::AlarmEvent> &currentAlarms,
    const QDateTime &capturedAtUtc) const
{
    return buildSnapshot(tagSnapshot.currentValues, currentAlarms, capturedAtUtc);
}

Monitor::Application::Dtos::DashboardSnapshot DashboardService::buildSnapshot(
    const QVector<Monitor::Domain::Tags::TagRuntimeState> &currentValues,
    const QVector<Monitor::Domain::Alarms::AlarmEvent> &currentAlarms,
    const QDateTime &capturedAtUtc) const
{
    using Monitor::Domain::Tags::TagQuality;

    Monitor::Domain::Common::UtcDateTime::require(capturedAtUtc, QStringLiteral("capturedAtUtc"));

    Monitor::Application::Dtos::DashboardSnapshot result;
    result.timestampUtc = capturedAtUtc;
    result.tags = currentValues;
    result.activeAlarms = currentAlarms;
    result.totalTagCount = currentValues.size();
    result.badQualityCount = static_cast<int>(std::count_if(currentValues.cbegin(), currentValues.cend(), [](const auto &state) {
        return state.quality != TagQuality::Good;
    }));

    const auto latestState = std::max_element(currentValues.cbegin(), currentValues.cend(), [](const auto &left, const auto &right) {
        return left.sequenceNo < right.sequenceNo;
    });

    if (latestState != currentValues.cend()) {
        result.sourceFrameId = latestState->sourceFrameId;
        result.sequenceNo = latestState->sequenceNo;
    }

    return result;
}

} // namespace Monitor::Application::Services
