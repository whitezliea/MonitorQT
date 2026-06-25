#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "navigation/NavigationPage.h"

#include <QMainWindow>
#include <QUuid>

#include <memory>

namespace Monitor::Application::Dtos {
struct UiSnapshot;
}

namespace Monitor::Application::Services {
class UiSnapshotProvider;
}

class BottomStatusBarWidget;
class AlarmCenterPageWidget;
class DashboardPageWidget;
class HistoryPageWidget;
class LogsSettingsPageWidget;
class MeasurementMapPageWidget;
class QLabel;
class NavigationService;
class QStackedWidget;
class QTimer;
class RealtimeTagsPageWidget;
class SideNavigationWidget;
class TopStatusBarWidget;
class TrendPageWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void handleCurrentPageChanged(NavigationPage page, const QString &title);
    void startMonitoring();
    void stopMonitoring();
    void refreshShellClock();
    void acknowledgeAlarm(const QUuid &alarmId);

private:
    void setupUi();
    void createPages();
    void applyApplicationStyle();
    void setRunningState(bool running);
    void refreshPages(const Monitor::Application::Dtos::UiSnapshot &snapshot);
    bool databaseReady() const;

    TopStatusBarWidget *m_topStatusBar = nullptr;
    SideNavigationWidget *m_sideNavigation = nullptr;
    QLabel *m_pageTitleLabel = nullptr;
    QStackedWidget *m_pageStack = nullptr;
    BottomStatusBarWidget *m_bottomStatusBar = nullptr;
    NavigationService *m_navigationService = nullptr;
    QTimer *m_shellTimer = nullptr;
    DashboardPageWidget *m_dashboardPage = nullptr;
    RealtimeTagsPageWidget *m_realtimeTagsPage = nullptr;
    TrendPageWidget *m_trendPage = nullptr;
    AlarmCenterPageWidget *m_alarmCenterPage = nullptr;
    HistoryPageWidget *m_historyPage = nullptr;
    MeasurementMapPageWidget *m_measurementMapPage = nullptr;
    LogsSettingsPageWidget *m_logsSettingsPage = nullptr;
    std::unique_ptr<Monitor::Application::Services::UiSnapshotProvider> m_snapshotProvider;

    bool m_running = false;
    quint64 m_lastFrame = 0;
};
#endif // MAINWINDOW_H
