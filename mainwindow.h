#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "navigation/NavigationPage.h"

#include <QMainWindow>

class BottomStatusBarWidget;
class QLabel;
class NavigationService;
class QStackedWidget;
class QTimer;
class SideNavigationWidget;
class TopStatusBarWidget;

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

private:
    void setupUi();
    void createPages();
    void applyApplicationStyle();
    void setRunningState(bool running);

    TopStatusBarWidget *m_topStatusBar = nullptr;
    SideNavigationWidget *m_sideNavigation = nullptr;
    QLabel *m_pageTitleLabel = nullptr;
    QStackedWidget *m_pageStack = nullptr;
    BottomStatusBarWidget *m_bottomStatusBar = nullptr;
    NavigationService *m_navigationService = nullptr;
    QTimer *m_shellTimer = nullptr;

    bool m_running = false;
    quint64 m_lastFrame = 0;
};
#endif // MAINWINDOW_H
