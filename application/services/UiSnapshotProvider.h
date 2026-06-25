#ifndef UISNAPSHOTPROVIDER_H
#define UISNAPSHOTPROVIDER_H

#include "application/dto/ApplicationDtos.h"
#include "application/pipelines/DataCleanPipeline.h"
#include "application/services/AlarmService.h"
#include "application/services/ChartDataService.h"
#include "application/services/DashboardService.h"
#include "application/services/MeasurementMapService.h"
#include "application/services/TagService.h"

namespace Monitor::Application::Services {

class UiSnapshotProvider
{
public:
    UiSnapshotProvider();

    void setRunning(bool running);
    bool isRunning() const;
    bool acknowledgeAlarm(const QUuid &alarmId);
    void saveRuntimeOptions(const Monitor::Application::Configuration::MonitorRuntimeOptions &options);
    void saveTagConfigurations(
        const QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> &configurations);

    Monitor::Application::Dtos::UiSnapshot refresh(bool databaseConnected);

private:
    Monitor::Domain::Measurements::RawMeasurementFrame nextFrame(const QDateTime &timestampUtc);
    void processFrame(const Monitor::Domain::Measurements::RawMeasurementFrame &frame);
    void appendLog(
        Monitor::Domain::Logs::OperationLogLevel level,
        const QString &category,
        const QString &action,
        const QString &message,
        const std::optional<QString> &detail = std::nullopt);
    void trimBuffers();

    Monitor::Application::Configuration::MonitorRuntimeOptions m_options;
    QVector<Monitor::Domain::Tags::TagDefinition> m_definitions;
    QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> m_configurations;
    Monitor::Application::Pipelines::DataCleanPipeline m_pipeline;
    Monitor::Application::Services::TagService m_tagService;
    Monitor::Application::Services::AlarmService m_alarmService;
    Monitor::Application::Services::DashboardService m_dashboardService;
    Monitor::Application::Services::ChartDataService m_chartDataService;
    Monitor::Application::Services::MeasurementMapService m_measurementMapService;
    QVector<Monitor::Domain::Tags::TagValue> m_historySamples;
    QVector<Monitor::Domain::Logs::OperationLog> m_operationLogs;
    quint64 m_sequenceNo = 0;
    bool m_running = false;
};

} // namespace Monitor::Application::Services

#endif // UISNAPSHOTPROVIDER_H
