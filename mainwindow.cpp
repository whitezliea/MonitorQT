#include "mainwindow.h"

#include "application/configuration/MonitorRuntimeOptions.h"
#include "application/services/UiSnapshotProvider.h"
#include "application/services/TagDefinitionCatalog.h"
#include "navigation/NavigationService.h"
#include "presentation/pages/MonitoringPages.h"
#include "shell/BottomStatusBarWidget.h"
#include "shell/SideNavigationWidget.h"
#include "shell/TopStatusBarWidget.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace {

Monitor::Application::Configuration::MonitorRuntimeOptions defaultRuntimeOptions()
{
    return Monitor::Application::Configuration::MonitorRuntimeOptions();
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_snapshotProvider(std::make_unique<Monitor::Application::Services::UiSnapshotProvider>())
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
    m_shellTimer->start(defaultRuntimeOptions().uiRefreshIntervalMs);
}

MainWindow::~MainWindow() = default;

void MainWindow::handleCurrentPageChanged(NavigationPage page, const QString &title)
{
    m_pageTitleLabel->setText(title);
    m_sideNavigation->setCurrentPage(page);
}

void MainWindow::startMonitoring()
{
    m_snapshotProvider->setRunning(true);
    setRunningState(true);
    refreshShellClock();
}

void MainWindow::stopMonitoring()
{
    m_snapshotProvider->setRunning(false);
    setRunningState(false);
    refreshShellClock();
}

void MainWindow::refreshShellClock()
{
    const auto snapshot = m_snapshotProvider->refresh(databaseReady());
    m_lastFrame = snapshot.shell.lastFrameIndex;
    m_bottomStatusBar->setDataSourceConnected(snapshot.shell.dataSourceConnected);
    m_bottomStatusBar->setDatabaseConnected(snapshot.shell.databaseConnected);
    m_bottomStatusBar->setLastFrame(snapshot.shell.lastFrameIndex);
    m_bottomStatusBar->setMatrixFrame(snapshot.shell.matrixFrameIndex);
    m_bottomStatusBar->setSyncState(snapshot.shell.syncState);
    m_bottomStatusBar->setCurrentTime(QDateTime::currentDateTime());
    refreshPages(snapshot);
}

void MainWindow::acknowledgeAlarm(const QUuid &alarmId)
{
    if (m_snapshotProvider->acknowledgeAlarm(alarmId)) {
        refreshShellClock();
    }
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
    m_shellTimer->setInterval(defaultRuntimeOptions().uiRefreshIntervalMs);
    m_topStatusBar->setDeviceId(Monitor::Application::Services::TagDefinitionCatalog::defaultDeviceId());
    m_bottomStatusBar->setRefreshIntervalMs(defaultRuntimeOptions().uiRefreshIntervalMs);
}

void MainWindow::createPages()
{
    m_dashboardPage = new DashboardPageWidget(this);
    m_realtimeTagsPage = new RealtimeTagsPageWidget(this);
    m_trendPage = new TrendPageWidget(this);
    m_alarmCenterPage = new AlarmCenterPageWidget(this);
    m_historyPage = new HistoryPageWidget(this);
    m_measurementMapPage = new MeasurementMapPageWidget(this);
    m_logsSettingsPage = new LogsSettingsPageWidget(this);

    connect(m_alarmCenterPage, &AlarmCenterPageWidget::acknowledgeRequested,
            this, &MainWindow::acknowledgeAlarm);
    connect(m_logsSettingsPage, &LogsSettingsPageWidget::runtimeOptionsSaveRequested,
            this, [this](const Monitor::Application::Configuration::MonitorRuntimeOptions &options) {
                m_snapshotProvider->saveRuntimeOptions(options);
                m_shellTimer->setInterval(options.uiRefreshIntervalMs);
                m_bottomStatusBar->setRefreshIntervalMs(options.uiRefreshIntervalMs);
                refreshShellClock();
            });
    connect(m_logsSettingsPage, &LogsSettingsPageWidget::tagConfigurationsSaveRequested,
            this, [this](const QVector<Monitor::Application::Configuration::TagRuntimeConfiguration> &configurations) {
                m_snapshotProvider->saveTagConfigurations(configurations);
                refreshShellClock();
            });

    m_navigationService->registerPage(NavigationPage::Dashboard, m_dashboardPage);
    m_navigationService->registerPage(NavigationPage::RealtimeTags, m_realtimeTagsPage);
    m_navigationService->registerPage(NavigationPage::Trend, m_trendPage);
    m_navigationService->registerPage(NavigationPage::AlarmCenter, m_alarmCenterPage);
    m_navigationService->registerPage(NavigationPage::History, m_historyPage);
    m_navigationService->registerPage(NavigationPage::MeasurementMap, m_measurementMapPage);
    m_navigationService->registerPage(NavigationPage::LogsSettings, m_logsSettingsPage);
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

        QLabel {
            color: #172033;
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

        QWidget#bottomStatusBar QLabel {
            color: #475569;
        }

        QFrame#summaryCard {
            background: #F8FAFC;
            border: 1px solid #D9E2EA;
            border-radius: 8px;
        }

        QLabel#cardTitle {
            color: #64748B;
            font-size: 12px;
            font-weight: 600;
        }

        QLabel#cardValue {
            color: #0F172A;
            font-size: 22px;
            font-weight: 700;
        }

        QTableWidget {
            background: #FBFCFE;
            alternate-background-color: #F1F5F9;
            border: 1px solid #D9E2EA;
            color: #1E293B;
            gridline-color: #E2E8F0;
            selection-background-color: #CFE3F0;
            selection-color: #0F172A;
        }

        QTableWidget:disabled {
            background: #FBFCFE;
            color: #334155;
        }

        QTableWidget::item {
            color: #1E293B;
        }

        QTableWidget::item:alternate {
            background: #F1F5F9;
        }

        QTableWidget::item:selected {
            background: #CFE3F0;
            color: #0F172A;
        }

        QTableWidget::item:disabled {
            color: #334155;
        }

        QHeaderView::section {
            background: #EEF3F6;
            color: #334155;
            border: 0;
            border-right: 1px solid #D9E2EA;
            border-bottom: 1px solid #D9E2EA;
            padding: 6px;
            font-weight: 700;
        }

        QLineEdit, QComboBox, QSpinBox {
            min-height: 30px;
            border: 1px solid #CBD5E1;
            border-radius: 6px;
            padding: 4px 8px;
            background: #FFFFFF;
            color: #1E293B;
            selection-background-color: #CFE3F0;
            selection-color: #0F172A;
        }

        QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled {
            background: #F8FAFC;
            color: #475569;
        }

        QComboBox QAbstractItemView {
            background: #FFFFFF;
            color: #1E293B;
            border: 1px solid #CBD5E1;
            outline: 0;
            selection-background-color: #CFE3F0;
            selection-color: #0F172A;
        }

        QComboBox QAbstractItemView::item {
            min-height: 28px;
            padding: 6px 8px;
            color: #1E293B;
            background: #FFFFFF;
        }

        QComboBox QAbstractItemView::item:hover,
        QComboBox QAbstractItemView::item:selected {
            background: #CFE3F0;
            color: #0F172A;
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

void MainWindow::refreshPages(const Monitor::Application::Dtos::UiSnapshot &snapshot)
{
    m_dashboardPage->refresh(snapshot);
    m_realtimeTagsPage->refresh(snapshot);
    m_trendPage->refresh(snapshot);
    m_alarmCenterPage->refresh(snapshot);
    m_historyPage->refresh(snapshot);
    m_measurementMapPage->refresh(snapshot);
    m_logsSettingsPage->refresh(snapshot);
}

bool MainWindow::databaseReady() const
{
    const auto path = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("data/multichannel-monitor.db"));
    return QFileInfo::exists(path);
}
