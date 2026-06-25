#include "ChartDataService.h"

#include <algorithm>
#include <stdexcept>

namespace Monitor::Application::Services {

ChartDataService::ChartDataService(const TagService *tagService)
    : m_tagService(tagService)
{
}

Monitor::Application::Dtos::TrendSeries ChartDataService::trendSeries(
    const QString &tagId,
    int pointCount) const
{
    if (!m_tagService) {
        throw std::logic_error("ChartDataService requires a TagService for trendSeries().");
    }

    return buildTrendSeries(m_tagService->snapshot(), tagId, pointCount);
}

Monitor::Application::Dtos::TrendSeries ChartDataService::buildTrendSeries(
    const Monitor::Domain::Tags::TagSnapshot &snapshot,
    const QString &tagId,
    int pointCount,
    const QVector<Monitor::Domain::Tags::TagRuntimeState> &currentValues,
    const std::optional<QDateTime> &maximumTimestampUtc) const
{
    if (tagId.trimmed().isEmpty()) {
        throw std::invalid_argument("TagId must not be empty.");
    }

    Monitor::Application::Dtos::TrendSeries result;
    result.tagId = tagId;
    result.requestedPointCount = std::max(pointCount, 0);

    if (pointCount > 0) {
        const auto bufferIt = snapshot.recentBuffers.constFind(tagId);
        if (bufferIt != snapshot.recentBuffers.cend()) {
            QVector<Monitor::Domain::Tags::TrendPoint> filtered;
            for (const auto &point : bufferIt.value()) {
                if (!maximumTimestampUtc.has_value() || point.timestampUtc <= maximumTimestampUtc.value()) {
                    filtered.append(point);
                }
            }

            const auto start = std::max<qsizetype>(0, filtered.size() - pointCount);
            result.points.reserve(filtered.size() - start);
            for (auto index = start; index < filtered.size(); ++index) {
                result.points.append(Monitor::Application::Dtos::TrendPointDto{
                    filtered.at(index).timestampUtc,
                    filtered.at(index).value,
                    filtered.at(index).quality,
                    false
                });
            }
        }
    }

    const auto &stateSource = currentValues.isEmpty() ? snapshot.currentValues : currentValues;
    const auto stateIt = std::find_if(stateSource.cbegin(), stateSource.cend(), [&tagId](const auto &state) {
        return state.tagId == tagId;
    });

    if (stateIt != stateSource.cend()) {
        result.sourceFrameId = stateIt->sourceFrameId;
        result.sequenceNo = stateIt->sequenceNo;
        result.sourceTimestampUtc = stateIt->timestampUtc;
    }

    return result;
}

} // namespace Monitor::Application::Services
