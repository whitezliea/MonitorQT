#ifndef CHARTDATASERVICE_H
#define CHARTDATASERVICE_H

#include "application/dto/ApplicationDtos.h"
#include "application/services/TagService.h"
#include "domain/tags/TagModels.h"

#include <optional>

namespace Monitor::Application::Services {

class ChartDataService
{
public:
    explicit ChartDataService(const TagService *tagService = nullptr);

    Monitor::Application::Dtos::TrendSeries trendSeries(
        const QString &tagId,
        int pointCount) const;
    Monitor::Application::Dtos::TrendSeries buildTrendSeries(
        const Monitor::Domain::Tags::TagSnapshot &snapshot,
        const QString &tagId,
        int pointCount,
        const QVector<Monitor::Domain::Tags::TagRuntimeState> &currentValues = {},
        const std::optional<QDateTime> &maximumTimestampUtc = std::nullopt) const;

private:
    const TagService *m_tagService = nullptr;
};

} // namespace Monitor::Application::Services

#endif // CHARTDATASERVICE_H
