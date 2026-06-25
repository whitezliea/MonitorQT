#include "DomainLayer.h"

#include "common/DomainCommon.h"
#include "devices/DeviceModels.h"
#include "measurements/MeasurementModels.h"
#include "rules/DomainRules.h"
#include "tags/TagModels.h"

#include <cmath>
#include <limits>

namespace Monitor::Domain {
namespace {

bool nearlyEqual(double left, double right, double epsilon = 1e-9)
{
    return std::abs(left - right) <= epsilon;
}

void addError(QStringList *errors, const QString &message)
{
    errors->append(message);
}

QDateTime utcDateTime(qint64 msecsSinceEpoch)
{
    return Common::UtcDateTime::fromCSharpTicks(
        Common::UtcDateTime::CSharpTicksAtUnixEpoch + msecsSinceEpoch * Common::UtcDateTime::TicksPerMillisecond);
}

} // namespace

DomainLayerInfo domainLayerInfo()
{
    return {
        QStringLiteral("MonitorDomain"),
        {
            QStringLiteral("devices"),
            QStringLiteral("measurements"),
            QStringLiteral("tags"),
            QStringLiteral("alarms"),
            QStringLiteral("rules"),
            QStringLiteral("logs"),
            QStringLiteral("tasks")
        },
        {
            QStringLiteral("QObject"),
            QStringLiteral("QWidget"),
            QStringLiteral("SQLite"),
            QStringLiteral("UI"),
            QStringLiteral("Simulator")
        }
    };
}

QStringList validateDomainLayer()
{
    QStringList errors;

    try {
        const auto timestamp = utcDateTime(0);
        auto frame = Measurements::MatrixFrame::fromRows(
            QUuid::createUuid(),
            timestamp,
            {
                {1.0, 2.0},
                {3.0, 4.0}
            });
        const auto statistics = frame.calculateStatistics();

        if (!nearlyEqual(statistics.minValue, 1.0) ||
            !nearlyEqual(statistics.maxValue, 4.0) ||
            !nearlyEqual(statistics.averageValue, 2.5) ||
            !nearlyEqual(statistics.stdDev, std::sqrt(1.25)) ||
            !nearlyEqual(statistics.uniformityMinMax, 0.25) ||
            !nearlyEqual(statistics.uniformity(), statistics.uniformityMinMax) ||
            !nearlyEqual(statistics.uniformityMinAverage, 0.4) ||
            statistics.validCount != 4 ||
            statistics.invalidCount != 0) {
            addError(&errors, QStringLiteral("Matrix statistics do not match the C# domain rule."));
        }

        auto invalidFrame = Measurements::MatrixFrame::fromRows(
            QUuid::createUuid(),
            timestamp,
            {
                {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}
            });
        const auto invalidStatistics = invalidFrame.calculateStatistics();
        if (!std::isnan(invalidStatistics.minValue) ||
            !std::isnan(invalidStatistics.maxValue) ||
            invalidStatistics.validCount != 0 ||
            invalidStatistics.invalidCount != 2) {
            addError(&errors, QStringLiteral("Matrix statistics must ignore non-finite values and return NaN when all values are invalid."));
        }

        Tags::TagDefinition definition;
        definition.tagId = QStringLiteral("MEAS.VIBRATION.CH01");
        definition.minValue = 0.0;
        definition.maxValue = 10.0;
        definition.warningHigh = 1.0;
        definition.alarmHigh = 2.5;

        const auto quality = Rules::QualityRule::fromDeviceState(Devices::DeviceStatus::Running, 0);
        if (quality != Tags::TagQuality::Good) {
            addError(&errors, QStringLiteral("QualityRule must map running device without error to Good."));
        }

        if (Rules::QualityRule::fromDeviceState(Devices::DeviceStatus::Offline, 0) != Tags::TagQuality::Offline) {
            addError(&errors, QStringLiteral("QualityRule must map Offline device to Offline quality."));
        }

        if (Rules::TagValidationRule::validateRange(11.0, definition) != Tags::TagQuality::OutOfRange ||
            Rules::TagValidationRule::validateRange(6.0, definition) != Tags::TagQuality::Good) {
            addError(&errors, QStringLiteral("TagValidationRule range behavior is not aligned."));
        }

        const auto alarmDefinition = Alarms::AlarmDefinition::fromTagDefinition(definition);
        if (Rules::AlarmRule::evaluate(6.0, Tags::TagQuality::Good, alarmDefinition) != Tags::TagAlarmState::AlarmHigh ||
            Rules::AlarmRule::evaluate(6.0, Tags::TagQuality::Bad, alarmDefinition) != Tags::TagAlarmState::Invalid ||
            Rules::AlarmRule::evaluate(6.0, Tags::TagQuality::Offline, alarmDefinition) != Tags::TagAlarmState::Offline) {
            addError(&errors, QStringLiteral("AlarmRule threshold and quality behavior is not aligned."));
        }

        Measurements::RawMeasurementFrame rawFrame;
        rawFrame.frameId = QUuid::createUuid();
        rawFrame.deviceId = QStringLiteral("MCMD-001");
        rawFrame.sequenceNo = 1;
        rawFrame.timestampUtc = timestamp;
        rawFrame.deviceStatus = Devices::DeviceStatus::Running;
        rawFrame.matrixValues = frame;
        rawFrame.errorCode = 0;
        rawFrame.quality = Tags::TagQuality::Good;
        Measurements::MeasurementTimeContract::validate(rawFrame);

        rawFrame.matrixValues->timestampUtc = timestamp.addMSecs(1);
        auto rejectedMismatchedMatrixTimestamp = false;
        try {
            Measurements::MeasurementTimeContract::validate(rawFrame);
        } catch (const Common::DomainException &) {
            rejectedMismatchedMatrixTimestamp = true;
        }

        if (!rejectedMismatchedMatrixTimestamp) {
            addError(&errors, QStringLiteral("MeasurementTimeContract must reject matrix timestamps that differ from the raw frame."));
        }
    } catch (const std::exception &exception) {
        addError(&errors, QStringLiteral("Domain validation threw unexpectedly: %1").arg(QString::fromUtf8(exception.what())));
    }

    return errors;
}

} // namespace Monitor::Domain
