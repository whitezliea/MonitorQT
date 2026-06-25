#include "mainwindow.h"

#include "navigation/NavigationService.h"
#include "pages/PagePlaceholderWidget.h"
#include "shell/BottomStatusBarWidget.h"
#include "shell/SideNavigationWidget.h"
#include "shell/TopStatusBarWidget.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    createPages();
    applyApplicationStyle();

    connect(m_sideNavigation, &SideNavigationWidget::navigationRequested,
            m_navigationService, &NavigationService::navigateTo);
    connect(m_sideNavigation, &SideNavigationWidget::startRequested,
            this, &MainWindow::startMonitoring);
    connect(m_sideNavigation, &SideNavigationWidget::stopRequested,
            this, &MainWindow::stopMonitoring);
    connect(m_navigationService, &NavigationService::currentPageChanged,
            this, &MainWindow::handleCurrentPageChanged);
    connect(m_shellTimer, &QTimer::timeout,
            this, &MainWindow::refreshShellClock);

    setWindowTitle(tr("MultiChannel Monitor"));
    setMinimumSize(1100, 720);
    resize(1280, 800);

    setRunningState(false);
    refreshShellClock();
    m_navigationService->navigateTo(NavigationPage::Dashboard);
    m_shellTimer->start(RefreshIntervalMs);
}

MainWindow::~MainWindow() = default;

void MainWindow::handleCurrentPageChanged(NavigationPage page, const QString &title)
{
    m_pageTitleLabel->setText(title);
    m_sideNavigation->setCurrentPage(page);
}

void MainWindow::startMonitoring()
{
    setRunningState(true);
}

void MainWindow::stopMonitoring()
{
    setRunningState(false);
}

void MainWindow::refreshShellClock()
{
    if (m_running) {
        ++m_lastFrame;
        m_bottomStatusBar->setLastFrame(m_lastFrame);
    }

    m_bottomStatusBar->setCurrentTime(QDateTime::currentDateTime());
}

void MainWindow::setupUi()
{
    auto *centralWidget = new QWidget(this);
    centralWidget->setObjectName(QStringLiteral("mainWindowRoot"));
    setCentralWidget(centralWidget);

    auto *rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_topStatusBar = new TopStatusBarWidget(this);
    rootLayout->addWidget(m_topStatusBar);

    auto *bodyWidget = new QWidget(this);
    bodyWidget->setObjectName(QStringLiteral("bodyArea"));
    auto *bodyLayout = new QHBoxLayout(bodyWidget);
    bodyLayout->setContentsMargins(18, 18, 18, 18);
    bodyLayout->setSpacing(18);

    m_sideNavigation = new SideNavigationWidget(this);
    bodyLayout->addWidget(m_sideNavigation);

    auto *contentWidget = new QWidget(this);
    contentWidget->setObjectName(QStringLiteral("contentArea"));
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(24, 20, 24, 24);
    contentLayout->setSpacing(16);

    m_pageTitleLabel = new QLabel(this);
    m_pageTitleLabel->setObjectName(QStringLiteral("pageTitleLabel"));
    contentLayout->addWidget(m_pageTitleLabel);

    auto *divider = new QFrame(this);
    divider->setObjectName(QStringLiteral("contentDivider"));
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Plain);
    contentLayout->addWidget(divider);

    m_pageStack = new QStackedWidget(this);
    m_pageStack->setObjectName(QStringLiteral("pageStack"));
    contentLayout->addWidget(m_pageStack, 1);

    bodyLayout->addWidget(contentWidget, 1);
    rootLayout->addWidget(bodyWidget, 1);

    m_bottomStatusBar = new BottomStatusBarWidget(this);
    rootLayout->addWidget(m_bottomStatusBar);

    m_navigationService = new NavigationService(m_pageStack, this);
    m_shellTimer = new QTimer(this);
    m_shellTimer->setInterval(RefreshIntervalMs);
}

void MainWindow::createPages()
{
    m_navigationService->registerPage(NavigationPage::Dashboard,
                                      new PagePlaceholderWidget(NavigationPage::Dashboard, this));
    m_navigationService->registerPage(NavigationPage::RealtimeTags,
                                      new PagePlaceholderWidget(NavigationPage::RealtimeTags, this));
    m_navigationService->registerPage(NavigationPage::Trend,
                                      new PagePlaceholderWidget(NavigationPage::Trend, this));
    m_navigationService->registerPage(NavigationPage::AlarmCenter,
                                      new PagePlaceholderWidget(NavigationPage::AlarmCenter, this));
    m_navigationService->registerPage(NavigationPage::History,
                                      new PagePlaceholderWidget(NavigationPage::History, this));
    m_navigationService->registerPage(NavigationPage::MeasurementMap,
                                      new PagePlaceholderWidget(NavigationPage::MeasurementMap, this));
    m_navigationService->registerPage(NavigationPage::LogsSettings,
                                      new PagePlaceholderWidget(NavigationPage::LogsSettings, this));
}

void MainWindow::applyApplicationStyle()
{
    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget#mainWindowRoot {
            background: #F3F6F8;
            color: #172033;
            font-family: "Segoe UI", "Microsoft YaHei", sans-serif;
            font-size: 13px;
        }

        QWidget#topStatusBar {
            background: #FFFFFF;
            border-bottom: 1px solid #D9E2EA;
        }

        QLabel#appTitleLabel {
            color: #111827;
            font-size: 18px;
            font-weight: 700;
        }

        QWidget#bodyArea {
            background: #EEF3F6;
        }

        QWidget#sideNavigation {
            background: #FFFFFF;
            border: 1px solid #D9E2EA;
            border-radius: 8px;
        }

        QLabel#brandLabel {
            color: #111827;
            font-size: 20px;
            font-weight: 700;
            line-height: 120%;
        }

        QListWidget#navigationList {
            background: transparent;
            outline: 0;
        }

        QListWidget#navigationList::item {
            color: #475569;
            border-radius: 6px;
            padding: 9px 12px;
        }

        QListWidget#navigationList::item:selected {
            background: #DDEAF3;
            color: #0F4C81;
            font-weight: 700;
        }

        QListWidget#navigationList::item:hover {
            background: #EDF4F8;
        }

        QPushButton {
            min-height: 32px;
            border-radius: 6px;
            padding: 6px 14px;
            font-weight: 600;
        }

        QPushButton#primaryButton {
            background: #256D85;
            color: #FFFFFF;
            border: 1px solid #256D85;
        }

        QPushButton#primaryButton:hover {
            background: #1F5F75;
        }

        QPushButton#secondaryButton {
            background: #FFFFFF;
            color: #334155;
            border: 1px solid #CBD5E1;
        }

        QPushButton#secondaryButton:hover {
            background: #F8FAFC;
        }

        QWidget#contentArea {
            background: #FFFFFF;
            border: 1px solid #D9E2EA;
            border-radius: 8px;
        }

        QLabel#pageTitleLabel {
            color: #111827;
            font-size: 22px;
            font-weight: 700;
        }

        QFrame#contentDivider {
            color: #D9E2EA;
            background: #D9E2EA;
            max-height: 1px;
        }

        QWidget#pageSurface {
            background: #FFFFFF;
        }

        QWidget#bottomStatusBar {
            background: #FFFFFF;
            border-top: 1px solid #D9E2EA;
            color: #475569;
        }
    )"));
}

void MainWindow::setRunningState(bool running)
{
    m_running = running;
    m_topStatusBar->setRunning(running);
    m_topStatusBar->setAcquisitionConnected(running);
    m_bottomStatusBar->setDataSourceConnected(running);
}
