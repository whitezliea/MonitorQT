#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "navigation/NavigationPage.h"

#include <QMainWindow>
#include <QStringList>
#include <QUuid>

namespace Monitor::Application::Dtos {
struct UiSnapshot;
}

namespace Monitor::Application::Services {
class RuntimeCommandFacade;
class RuntimeUiSnapshotProvider;
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
    explicit MainWindow(
        Monitor::Application::Services::RuntimeCommandFacade *runtimeCommands,
        Monitor::Application::Services::RuntimeUiSnapshotProvider *snapshotProvider,
        QWidget *parent = nullptr);
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
    void reportCommandErrors(const QString &action, const QStringList &errors);
    void refreshPages(const Monitor::Application::Dtos::UiSnapshot &snapshot);

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
    Monitor::Application::Services::RuntimeCommandFacade *m_runtimeCommands = nullptr;
    Monitor::Application::Services::RuntimeUiSnapshotProvider *m_snapshotProvider = nullptr;

    bool m_running = false;
    quint64 m_lastFrame = 0;
};
#endif // MAINWINDOW_H
