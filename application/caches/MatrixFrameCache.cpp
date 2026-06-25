#include "MatrixFrameCache.h"

#include <QMutexLocker>

namespace Monitor::Application::Caches {

void MatrixFrameCache::update(const Monitor::Domain::Measurements::MatrixFrame &frame)
{
    Monitor::Domain::Measurements::MeasurementTimeContract::validate(frame);

    QMutexLocker locker(&m_mutex);
    m_latestFrame = frame;
}

std::optional<Monitor::Domain::Measurements::MatrixFrame> MatrixFrameCache::latest() const
{
    QMutexLocker locker(&m_mutex);
    return m_latestFrame;
}

} // namespace Monitor::Application::Caches
