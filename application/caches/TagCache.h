#ifndef TAGCACHE_H
#define TAGCACHE_H

#include "domain/tags/TagModels.h"

#include <QHash>
#include <QMutex>
#include <QString>
#include <QVector>

namespace Monitor::Application::Caches {

class TagCache
{
public:
    explicit TagCache(int trendBufferCapacity);

    int trendBufferCapacity() const;
    void update(const QVector<Monitor::Domain::Tags::TagRuntimeState> &values);
    Monitor::Domain::Tags::TagSnapshot snapshot() const;

private:
    int m_trendBufferCapacity = 0;
    mutable QMutex m_mutex;
    QHash<QString, Monitor::Domain::Tags::TagRuntimeState> m_currentValues;
    QHash<QString, QVector<Monitor::Domain::Tags::TrendPoint>> m_recentBuffers;
};

} // namespace Monitor::Application::Caches

#endif // TAGCACHE_H
