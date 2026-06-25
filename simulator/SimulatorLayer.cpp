#include "SimulatorLayer.h"

#include "adapters/SimulatorDataSource.h"
#include "domain/common/DomainCommon.h"
#include "domain/devices/DeviceModels.h"
#include "domain/tags/TagModels.h"
#include "generators/FakeDataGenerator.h"
#include "scenarios/SimulationScenario.h"

#include <QDateTime>
#include <QTimeZone>

#include <cmath>
#include <exception>

namespace Monitor::Simulator {
namespace {

QDateTime utcDateTime(qint64 msecsSinceEpoch)
{
    return Monitor::Domain::Common::UtcDateTime::fromCSharpTicks(
        Monitor::Domain::Common::UtcDateTime::CSharpTicksAtUnixEpoch
        + msecsSinceEpoch * Monitor::Domain::Common::UtcDateTime::TicksPerMillisecond);
}

const Monitor::Domain::Measurements::ChannelValue *findChannel(
    const QVector<Monitor::Domain::Measurements::ChannelValue> &channels,
    const QString &channelId)
{
    for (const auto &channel : channels) {
        if (channel.channelId == channelId) {
            return &channel;
        }
    }

    return nullptr;
}

void addError(QStringList *errors, const QString &message)
{
    errors->append(message);
}

} // namespace

SimulatorLayerInfo simulatorLayerInfo()
{
    return {
        QStringLiteral("MonitorSimulator"),
        {
            QStringLiteral("adapters"),
            QStringLiteral("generators"),
            QStringLiteral("models"),
            QStringLiteral("noise"),
            QStringLiteral("profiles"),
            QStringLiteral("scenarios")
        },
        {
            QStringLiteral("MonitorApplication"),
            QStringLiteral("MonitorDomain"),
            QStringLiteral("QtCore")
        },
        {
            QStringLiteral("QtWidgets"),
            QStringLiteral("QtSql"),
            QStringLiteral("Presentation")
        }
    };
}

QStringList validateSimulatorLayer()
{
    QStringList errors;

    try {
        const auto start = utcDateTime(0);

        {
            Generators::FakeDataGenerator generator(
                QStringLiteral("MCMD-TEST"),
                Scenarios::SimulationScenario::normal(),
                start);
            const auto first = generator.nextFrame(start.addMSecs(500));
            const auto second = generator.nextFrame(start.addMSecs(1000));
            if (second.sequenceNo != first.sequenceNo + 1) {
                addError(&errors, QStringLiteral("FakeDataGenerator sequence number must increment by one."));
            }
        }

        {
            Generators::FakeDataGenerator generator(
                QStringLiteral("MCMD-TEST"),
                Scenarios::SimulationScenario::normal(),
                start);
            const auto frame = generator.nextFrame(start.addSecs(1));
            if (frame.deviceStatus != Monitor::Domain::Devices::DeviceStatus::Running ||
                frame.quality != Monitor::Domain::Tags::TagQuality::Good ||
                frame.channelValues.size() != 6 ||
                !frame.matrixValues.has_value() ||
                frame.matrixValues->rows != 16 ||
                frame.matrixValues->columns != 16) {
                addError(&errors, QStringLiteral("NormalScenario must generate a good running frame with six channels and a 16x16 matrix."));
            }

            for (const auto &channel : frame.channelValues) {
                if (channel.quality != Monitor::Domain::Tags::TagQuality::Good) {
                    addError(&errors, QStringLiteral("NormalScenario channels must all be Good quality."));
                    break;
                }
            }
        }

        {
            Generators::FakeDataGenerator normalGenerator(
                QStringLiteral("MCMD-TEST"),
                Scenarios::SimulationScenario::normal(),
                start);
            Generators::FakeDataGenerator demoGenerator(
                QStringLiteral("MCMD-TEST"),
                Scenarios::SimulationScenario::demo(),
                start);
            const auto normalFrame = normalGenerator.nextFrame(start.addSecs(21));
            const auto demoFrame = demoGenerator.nextFrame(start.addSecs(21));
            const auto *normalTemperature = findChannel(normalFrame.channelValues, QStringLiteral("TEMP_CH01"));
            const auto *demoTemperature = findChannel(demoFrame.channelValues, QStringLiteral("TEMP_CH01"));
            if (!normalTemperature || !demoTemperature ||
                !(demoTemperature->value > normalTemperature->value + 0.5)) {
                addError(&errors, QStringLiteral("DemoScenario must produce a temperature rise window."));
            }
        }

        {
            Generators::FakeDataGenerator generator(
                QStringLiteral("MCMD-TEST"),
                Scenarios::SimulationScenario::demo(),
                start);
            const auto frame = generator.nextFrame(start.addSecs(66));
            const auto *light = findChannel(frame.channelValues, QStringLiteral("LIGHT_CH01"));
            if (!light ||
                !std::isnan(light->value) ||
                light->quality != Monitor::Domain::Tags::TagQuality::DeviceError ||
                light->errorCode != 3001) {
                addError(&errors, QStringLiteral("DemoScenario must produce the light-channel device error window."));
            }
        }

        {
            Generators::FakeDataGenerator generator(
                QStringLiteral("MCMD-TEST"),
                Scenarios::SimulationScenario::offline(),
                start);
            const auto frame = generator.nextFrame(start.addSecs(1));
            if (frame.deviceStatus != Monitor::Domain::Devices::DeviceStatus::Offline ||
                frame.quality != Monitor::Domain::Tags::TagQuality::Offline) {
                addError(&errors, QStringLiteral("OfflineScenario must produce an offline frame."));
            }

            for (const auto &channel : frame.channelValues) {
                if (channel.quality != Monitor::Domain::Tags::TagQuality::Offline || !std::isnan(channel.value)) {
                    addError(&errors, QStringLiteral("OfflineScenario channels must be Offline with missing values."));
                    break;
                }
            }
        }

        {
            Generators::FakeDataGenerator normalGenerator(
                QStringLiteral("MCMD-TEST"),
                Scenarios::SimulationScenario::normal(),
                start);
            Generators::FakeDataGenerator hotspotGenerator(
                QStringLiteral("MCMD-TEST"),
                Scenarios::SimulationScenario::matrixHotspot(),
                start);
            const auto normalFrame = normalGenerator.nextFrame(start.addSecs(1));
            const auto hotspotFrame = hotspotGenerator.nextFrame(start.addSecs(1));
            const auto normalValue = normalFrame.matrixValues->valueAt(9, 10);
            const auto hotspotValue = hotspotFrame.matrixValues->valueAt(9, 10);
            if (!(hotspotValue > normalValue + 250.0)) {
                addError(&errors, QStringLiteral("MatrixHotspotScenario must increase the hotspot region."));
            }
        }

        {
            auto rejectedNonUtc = false;
            try {
                Generators::FakeDataGenerator generator(
                    QStringLiteral("MCMD-TEST"),
                    Scenarios::SimulationScenario::normal(),
                    start);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                generator.nextFrame(QDateTime(
                    QDate(2026, 6, 25),
                    QTime(12, 0, 0),
                    QTimeZone::fromSecondsAheadOfUtc(3600)));
#else
                generator.nextFrame(QDateTime(QDate(2026, 6, 25), QTime(12, 0, 0), Qt::OffsetFromUTC, 3600));
#endif
            } catch (const std::exception &) {
                rejectedNonUtc = true;
            }

            if (!rejectedNonUtc) {
                addError(&errors, QStringLiteral("FakeDataGenerator must reject non-UTC timestamps."));
            }
        }

        {
            Adapters::SimulatorDataSource source(
                Generators::FakeDataGenerator(
                    QStringLiteral("MCMD-TEST"),
                    Scenarios::SimulationScenario::normal(),
                    start.addSecs(-1)),
                Monitor::Application::Configuration::MonitorRuntimeOptions());
            const auto timestamp = start.addSecs(1);
            const auto frame = source.readNextFrame(timestamp);
            if (frame.timestampUtc != timestamp || !frame.matrixValues.has_value() || frame.matrixValues->timestampUtc != timestamp) {
                addError(&errors, QStringLiteral("SimulatorDataSource must use the injected timestamp for raw and matrix frames."));
            }

            source.cancel();
            if (!source.isCanceled()) {
                addError(&errors, QStringLiteral("SimulatorDataSource cancel flag must be observable."));
            }
        }
    } catch (const std::exception &exception) {
        addError(&errors, QStringLiteral("Simulator validation threw unexpectedly: %1").arg(QString::fromUtf8(exception.what())));
    }

    return errors;
}

} // namespace Monitor::Simulator
