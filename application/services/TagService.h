#ifndef TAGSERVICE_H
#define TAGSERVICE_H

#include "application/caches/TagCache.h"
#include "domain/tags/TagModels.h"

#include <QDateTime>
#include <QVector>

#include <memory>

namespace Monitor::Application::Services {

class TagService
{
public:
    explicit TagService(int trendBufferCapacity);
    explicit TagService(Monitor::Application::Caches::TagCache *tagCache);

    void updateTags(const QVector<Monitor::Domain::Tags::TagRuntimeState> &values);
    Monitor::Domain::Tags::TagSnapshot snapshot() const;

private:
    std::unique_ptr<Monitor::Application::Caches::TagCache> m_ownedCache;
    Monitor::Application::Caches::TagCache *m_cache = nullptr;
};

} // namespace Monitor::Application::Services

#endif // TAGSERVICE_H
