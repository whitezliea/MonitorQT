#ifndef IRAWFRAMESOURCE_H
#define IRAWFRAMESOURCE_H

#include "domain/measurements/MeasurementModels.h"

#include <QDateTime>

namespace Monitor::Application::Abstractions {

class IRawFrameSource
{
public:
    virtual ~IRawFrameSource() = default;

    virtual bool readNextFrame(
        const QDateTime &timestampUtc,
        Monitor::Domain::Measurements::RawMeasurementFrame *frame) = 0;
    virtual void cancel() = 0;
    virtual void resetCancellation() = 0;
};

} // namespace Monitor::Application::Abstractions

#endif // IRAWFRAMESOURCE_H
