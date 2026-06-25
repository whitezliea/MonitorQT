#include "TagService.h"

#include <memory>
#include <stdexcept>

namespace Monitor::Application::Services {

TagService::TagService(int trendBufferCapacity)
    : m_ownedCache(std::make_unique<Monitor::Application::Caches::TagCache>(trendBufferCapacity)),
      m_cache(m_ownedCache.get())
{
}

TagService::TagService(Monitor::Application::Caches::TagCache *tagCache)
    : m_cache(tagCache)
{
    if (!m_cache) {
        throw std::invalid_argument("TagCache must not be null.");
    }
}

void TagService::updateTags(const QVector<Monitor::Domain::Tags::TagRuntimeState> &values)
{
    m_cache->update(values);
}

Monitor::Domain::Tags::TagSnapshot TagService::snapshot() const
{
    return m_cache->snapshot();
}

} // namespace Monitor::Application::Services
