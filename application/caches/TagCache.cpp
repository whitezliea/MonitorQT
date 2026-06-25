#include "TagCache.h"

#include "domain/common/DomainCommon.h"

#include <QMutexLocker>

#include <algorithm>
#include <stdexcept>

namespace Monitor::Application::Caches {

TagCache::TagCache(int trendBufferCapacity)
    : m_trendBufferCapacity(trendBufferCapacity)
{
    if (trendBufferCapacity <= 0) {
        throw std::out_of_range("Trend buffer capacity must be greater than zero.");
    }
}

int TagCache::trendBufferCapacity() const
{
    return m_trendBufferCapacity;
}

void TagCache::update(const QVector<Monitor::Domain::Tags::TagRuntimeState> &values)
{
    using Monitor::Domain::Common::UtcDateTime;
    using Monitor::Domain::Tags::TrendPoint;

    QMutexLocker locker(&m_mutex);
    for (const auto &value : values) {
        UtcDateTime::require(value.timestampUtc, QStringLiteral("values.timestampUtc"));
        UtcDateTime::require(value.lastUpdateTimeUtc, QStringLiteral("values.lastUpdateTimeUtc"));

        m_currentValues.insert(value.tagId, value);

        if (!value.numericValue.has_value()) {
            continue;
        }

        auto &buffer = m_recentBuffers[value.tagId];
        buffer.append(TrendPoint{value.timestampUtc, value.numericValue.value(), value.quality});
        while (buffer.size() > m_trendBufferCapacity) {
            buffer.removeFirst();
        }
    }
}

Monitor::Domain::Tags::TagSnapshot TagCache::snapshot() const
{
    QMutexLocker locker(&m_mutex);

    QVector<Monitor::Domain::Tags::TagRuntimeState> values;
    values.reserve(m_currentValues.size());
    for (auto it = m_currentValues.cbegin(); it != m_currentValues.cend(); ++it) {
        values.append(it.value());
    }
    std::sort(values.begin(), values.end(), [](const auto &left, const auto &right) {
        return left.tagId < right.tagId;
    });

    return {values, m_recentBuffers};
}

} // namespace Monitor::Application::Caches
