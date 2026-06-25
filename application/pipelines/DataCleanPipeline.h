#ifndef DATACLEANPIPELINE_H
#define DATACLEANPIPELINE_H

#include "domain/measurements/MeasurementModels.h"
#include "domain/tags/TagModels.h"

#include <QHash>
#include <QString>
#include <QVector>

#include <optional>

namespace Monitor::Application::Pipelines {

class DataCleanPipeline
{
public:
    DataCleanPipeline();
    explicit DataCleanPipeline(
        const QVector<Monitor::Domain::Tags::TagDefinition> &definitions);
    DataCleanPipeline(
        const QVector<Monitor::Domain::Tags::TagDefinition> &definitions,
        const QVector<Monitor::Domain::Tags::TagSourceMapping> &mappings);

    QVector<Monitor::Domain::Tags::TagValue> clean(
        const Monitor::Domain::Measurements::RawMeasurementFrame &frame);
    QVector<Monitor::Domain::Tags::CleanedTagValue> cleanToCleanedValues(
        const Monitor::Domain::Measurements::RawMeasurementFrame &frame);
    QVector<Monitor::Domain::Tags::TagRuntimeState> toRuntimeStates(
        const Monitor::Domain::Measurements::RawMeasurementFrame &frame,
        const QDateTime &lastUpdateTimeUtc);

    void resetSession();

private:
    struct LastFrameInfo
    {
        QDateTime timestampUtc;
        qint64 sequenceNo = 0;
    };

    void addFrameFieldTags(
        const Monitor::Domain::Measurements::RawMeasurementFrame &frame,
        QVector<Monitor::Domain::Tags::CleanedTagValue> *values) const;
    void addChannelTags(
        const Monitor::Domain::Measurements::RawMeasurementFrame &frame,
        QVector<Monitor::Domain::Tags::CleanedTagValue> *values) const;
    void addDerivedTags(
        const Monitor::Domain::Measurements::RawMeasurementFrame &frame,
        QVector<Monitor::Domain::Tags::CleanedTagValue> *values) const;
    void addMatrixStatisticTags(
        const Monitor::Domain::Measurements::RawMeasurementFrame &frame,
        QVector<Monitor::Domain::Tags::CleanedTagValue> *values) const;

    Monitor::Domain::Tags::CleanedTagValue createCleanedValue(
        const Monitor::Domain::Measurements::RawMeasurementFrame &frame,
        const Monitor::Domain::Tags::TagDefinition &definition,
        const Monitor::Domain::Tags::TagSourceMapping &mapping,
        std::optional<double> numericValue,
        std::optional<QString> textValue,
        std::optional<bool> boolValue,
        Monitor::Domain::Tags::TagQuality quality,
        std::optional<QString> cleanMessage) const;
    Monitor::Domain::Tags::TagAlarmState evaluateAlarmState(
        double value,
        Monitor::Domain::Tags::TagQuality quality,
        const Monitor::Domain::Tags::TagDefinition *definition) const;

    QHash<QString, Monitor::Domain::Tags::TagDefinition> m_definitions;
    QVector<Monitor::Domain::Tags::TagSourceMapping> m_mappings;
    QHash<QString, LastFrameInfo> m_lastFrames;
};

} // namespace Monitor::Application::Pipelines

#endif // DATACLEANPIPELINE_H
