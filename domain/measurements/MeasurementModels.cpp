#include "MeasurementModels.h"

#include "domain/common/DomainCommon.h"
#include "domain/rules/MatrixStatisticsCalculator.h"

namespace Monitor::Domain::Measurements {

double MatrixStatistics::uniformity() const
{
    return uniformityMinMax;
}

MatrixFrame MatrixFrame::fromRows(
    const QUuid &frameId,
    const QDateTime &timestampUtc,
    const QVector<QVector<double>> &sourceRows,
    const QUuid &sourceFrameId,
    qint64 sequenceNo)
{
    MatrixFrame frame;
    frame.frameId = frameId;
    frame.timestampUtc = timestampUtc;
    frame.rows = sourceRows.size();
    frame.columns = sourceRows.isEmpty() ? 0 : sourceRows.first().size();
    frame.sourceFrameId = sourceFrameId;
    frame.sequenceNo = sequenceNo;
    frame.values.reserve(frame.rows * frame.columns);

    for (const auto &row : sourceRows) {
        if (row.size() != frame.columns) {
            throw Common::DomainException(QStringLiteral("Matrix rows must have the same column count."));
        }

        for (const auto value : row) {
            frame.values.append(value);
        }
    }

    return frame;
}

int MatrixFrame::valueCount() const
{
    return values.size();
}

bool MatrixFrame::dimensionsMatchValues() const
{
    return rows >= 0 && columns >= 0 && rows * columns == values.size();
}

double MatrixFrame::valueAt(int row, int column) const
{
    if (row < 0 || row >= rows || column < 0 || column >= columns) {
        throw Common::DomainException(QStringLiteral("Matrix index is out of range."));
    }

    return values.at(row * columns + column);
}

void MatrixFrame::setValueAt(int row, int column, double value)
{
    if (row < 0 || row >= rows || column < 0 || column >= columns) {
        throw Common::DomainException(QStringLiteral("Matrix index is out of range."));
    }

    values[row * columns + column] = value;
}

MatrixStatistics MatrixFrame::calculateStatistics() const
{
    if (!dimensionsMatchValues()) {
        throw Common::DomainException(
            QStringLiteral("Matrix dimensions %1x%2 do not match values count %3.")
                .arg(rows)
                .arg(columns)
                .arg(values.size()));
    }

    return Rules::MatrixStatisticsCalculator::calculate(values);
}

void MeasurementTimeContract::validate(const RawMeasurementFrame &frame)
{
    Common::UtcDateTime::require(frame.timestampUtc, QStringLiteral("RawMeasurementFrame.timestampUtc"));

    if (!frame.matrixValues.has_value()) {
        return;
    }

    validate(frame.matrixValues.value());
    if (frame.matrixValues->timestampUtc != frame.timestampUtc) {
        throw Common::DomainException(QStringLiteral("Matrix frame timestamp must match its source raw frame timestamp."));
    }
}

void MeasurementTimeContract::validate(const MatrixFrame &frame)
{
    Common::UtcDateTime::require(frame.timestampUtc, QStringLiteral("MatrixFrame.timestampUtc"));
}

} // namespace Monitor::Domain::Measurements
