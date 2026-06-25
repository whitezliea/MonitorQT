#ifndef MONITORINGPAGES_H
#define MONITORINGPAGES_H

#include "application/dto/ApplicationDtos.h"

#include <optional>

#include <QPoint>
#include <QRectF>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class TrendChartWidget : public QWidget
{
public:
    explicit TrendChartWidget(QWidget *parent = nullptr);
    void setSeries(
        const QVector<Monitor::Application::Dtos::TrendPointDto> &points,
        const std::optional<Monitor::Application::Configuration::TagRuntimeConfiguration> &configuration);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<Monitor::Application::Dtos::TrendPointDto> m_points;
    std::optional<Monitor::Application::Configuration::TagRuntimeConfiguration> m_configuration;
};

class HeatmapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HeatmapWidget(QWidget *parent = nullptr);
    void setSnapshot(const std::optional<Monitor::Application::Dtos::MeasurementMapSnapshot> &snapshot);
    void setRenderRange(const std::optional<Monitor::Application::Dtos::ScaleRange> &range);

signals:
    void cellSelected(int row, int column, double value);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QRectF heatmapArea() const;
    std::optional<Monitor::Application::Dtos::HeatmapCell> cellAtPosition(const QPoint &position) const;

    std::optional<Monitor::Application::Dtos::MeasurementMapSnapshot> m_snapshot;
    std::optional<Monitor::Application::Dtos::ScaleRange> m_renderRange;
    int m_selectedRow = -1;
    int m_selectedColumn = -1;
};

class DashboardPageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardPageWidget(QWidget *parent = nullptr);
    void refresh(const Monitor::Application::Dtos::UiSnapshot &snapshot);

private:
    QLabel *m_totalTagsLabel = nullptr;
    QLabel *m_badQualityLabel = nullptr;
    QLabel *m_activeAlarmsLabel = nullptr;
    QLabel *m_matrixLabel = nullptr;
    QTableWidget *m_keyTagsTable = nullptr;
    QTableWidget *m_alarmTable = nullptr;
    HeatmapWidget *m_previewMap = nullptr;
};

class RealtimeTagsPageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RealtimeTagsPageWidget(QWidget *parent = nullptr);
    void refresh(const Monitor::Application::Dtos::UiSnapshot &snapshot);

private:
    void applyFilters();

    Monitor::Application::Dtos::UiSnapshot m_snapshot;
    QLineEdit *m_filterEdit = nullptr;
    QComboBox *m_categoryCombo = nullptr;
    QTableWidget *m_table = nullptr;
};

class TrendPageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TrendPageWidget(QWidget *parent = nullptr);
    void refresh(const Monitor::Application::Dtos::UiSnapshot &snapshot);

private:
    void rebuildTagList(const Monitor::Application::Dtos::UiSnapshot &snapshot);
    void updateChart();
    void exportTrendRows();

    Monitor::Application::Dtos::UiSnapshot m_snapshot;
    QComboBox *m_tagCombo = nullptr;
    QComboBox *m_windowCombo = nullptr;
    QComboBox *m_sourceCombo = nullptr;
    QLabel *m_summaryLabel = nullptr;
    TrendChartWidget *m_chart = nullptr;
    QTableWidget *m_pointsTable = nullptr;
};

class AlarmCenterPageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AlarmCenterPageWidget(QWidget *parent = nullptr);
    void refresh(const Monitor::Application::Dtos::UiSnapshot &snapshot);

signals:
    void acknowledgeRequested(const QUuid &alarmId);

private:
    void acknowledgeSelected();
    void applyHistoryFilter();

    Monitor::Application::Dtos::UiSnapshot m_snapshot;
    QTableWidget *m_currentTable = nullptr;
    QLineEdit *m_historyFilterEdit = nullptr;
    QTableWidget *m_historyTable = nullptr;
};

class HistoryPageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryPageWidget(QWidget *parent = nullptr);
    void refresh(const Monitor::Application::Dtos::UiSnapshot &snapshot);

private:
    void rebuildTagList(const Monitor::Application::Dtos::UiSnapshot &snapshot);
    void applyQuery();
    void exportCurrentRows();

    Monitor::Application::Dtos::UiSnapshot m_snapshot;
    QComboBox *m_tagCombo = nullptr;
    QSpinBox *m_limitSpin = nullptr;
    QTableWidget *m_table = nullptr;
};

class MeasurementMapPageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MeasurementMapPageWidget(QWidget *parent = nullptr);
    void refresh(const Monitor::Application::Dtos::UiSnapshot &snapshot);

private:
    void updateHeatmapRenderOptions();
    void exportMatrixCsv();

    Monitor::Application::Dtos::UiSnapshot m_snapshot;
    QLabel *m_qualityLabel = nullptr;
    QLabel *m_rangeLabel = nullptr;
    QLabel *m_cellLabel = nullptr;
    QLabel *m_hotspotLabel = nullptr;
    QComboBox *m_scaleModeCombo = nullptr;
    QDoubleSpinBox *m_fixedMinSpin = nullptr;
    QDoubleSpinBox *m_fixedMaxSpin = nullptr;
    HeatmapWidget *m_heatmap = nullptr;
    QTableWidget *m_statsTable = nullptr;
    QTableWidget *m_abnormalTable = nullptr;
};

class LogsSettingsPageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LogsSettingsPageWidget(QWidget *parent = nullptr);
    void refresh(const Monitor::Application::Dtos::UiSnapshot &snapshot);

signals:
    void runtimeOptionsSaveRequested(const Monitor::Application::Configuration::MonitorRuntimeOptions &options);
    void tagConfigurationsSaveRequested(
        const QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> &configurations);

private:
    void saveRuntimeOptions();
    void saveTagConfigurations();
    void populateConfigurationRows();

    Monitor::Application::Dtos::UiSnapshot m_snapshot;
    QTableWidget *m_logTable = nullptr;
    QSpinBox *m_generateIntervalSpin = nullptr;
    QSpinBox *m_refreshIntervalSpin = nullptr;
    QSpinBox *m_historyRetentionSpin = nullptr;
    QTableWidget *m_configurationTable = nullptr;
};

#endif // MONITORINGPAGES_H
