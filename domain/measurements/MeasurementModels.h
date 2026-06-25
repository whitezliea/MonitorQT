#ifndef MEASUREMENTMODELS_H
#define MEASUREMENTMODELS_H

#include "domain/devices/DeviceModels.h"
#include "domain/tags/TagModels.h"

#include <QDateTime>
#include <QString>
#include <QUuid>
#include <QVector>

#include <optional>

namespace Monitor::Domain::Measurements {

enum class MeasurementQuality {
    Valid,
    Invalid,
    OutOfRange,
    Missing
};

struct ChannelValue
{
    QString channelId;
    double value = 0.0;
    QString unit;
    Tags::TagQuality quality = Tags::TagQuality::Good;
    int errorCode = 0;
};

struct MatrixStatistics
{
    double minValue = 0.0;
    double maxValue = 0.0;
    double averageValue = 0.0;
    double stdDev = 0.0;
    double uniformityMinMax = 0.0;
    double uniformityMinAverage = 0.0;
    int validCount = 0;
    int invalidCount = 0;

    double uniformity() const;
};

struct MatrixFrame
{
    QUuid frameId;
    QDateTime timestampUtc;
    int rows = 0;
    int columns = 0;
    QVector<double> values;
    QUuid sourceFrameId;
    qint64 sequenceNo = 0;

    static MatrixFrame fromRows(
        const QUuid &frameId,
        const QDateTime &timestampUtc,
        const QVector<QVector<double>> &rows,
        const QUuid &sourceFrameId = QUuid(),
        qint64 sequenceNo = 0);

    int valueCount() const;
    bool dimensionsMatchValues() const;
    double valueAt(int row, int column) const;
    void setValueAt(int row, int column, double value);
    MatrixStatistics calculateStatistics() const;
};

struct MatrixValue
{
    int row = 0;
    int column = 0;
    double value = 0.0;
    QString unit;
};

struct MeasurementPoint
{
    int row = 0;
    int column = 0;
    double value = 0.0;
};

struct RawChannelValue
{
    QString code;
    std::optional<double> value;
    QString unit;
    Tags::TagQuality quality = Tags::TagQuality::Good;
    int errorCode = 0;
};

struct RawMatrixFrame
{
    int rows = 0;
    int columns = 0;
    QString valueType;
    QString unit;
    QVector<double> values;
    Tags::TagQuality quality = Tags::TagQuality::Good;
    int errorCode = 0;
};

struct RawMeasurementFrame
{
    QUuid frameId;
    QString deviceId;
    qint64 sequenceNo = 0;
    QDateTime timestampUtc;
    Devices::DeviceStatus deviceStatus = Devices::DeviceStatus::Stopped;
    QVector<ChannelValue> channelValues;
    std::optional<MatrixFrame> matrixValues;
    int errorCode = 0;
    Tags::TagQuality quality = Tags::TagQuality::Good;
};

class MeasurementTimeContract
{
public:
    static void validate(const RawMeasurementFrame &frame);
    static void validate(const MatrixFrame &frame);
};

} // namespace Monitor::Domain::Measurements

#endif // MEASUREMENTMODELS_H
