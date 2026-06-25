#ifndef MATRIXFRAMECACHE_H
#define MATRIXFRAMECACHE_H

#include "domain/measurements/MeasurementModels.h"

#include <QMutex>

#include <optional>

namespace Monitor::Application::Caches {

class MatrixFrameCache
{
public:
    void update(const Monitor::Domain::Measurements::MatrixFrame &frame);
    std::optional<Monitor::Domain::Measurements::MatrixFrame> latest() const;

private:
    mutable QMutex m_mutex;
    std::optional<Monitor::Domain::Measurements::MatrixFrame> m_latestFrame;
};

} // namespace Monitor::Application::Caches

#endif // MATRIXFRAMECACHE_H
