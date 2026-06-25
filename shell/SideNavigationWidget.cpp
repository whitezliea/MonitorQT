#include "SideNavigationWidget.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

SideNavigationWidget::SideNavigationWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("sideNavigation"));
    setMinimumWidth(220);
    setMaximumWidth(260);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 18, 16, 16);
    layout->setSpacing(16);

    auto *brandLabel = new QLabel(tr("MultiChannel\nMonitor"), this);
    brandLabel->setObjectName(QStringLiteral("brandLabel"));
    layout->addWidget(brandLabel);

    m_navigationList = new QListWidget(this);
    m_navigationList->setObjectName(QStringLiteral("navigationList"));
    m_navigationList->setFrameShape(QFrame::NoFrame);
    m_navigationList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_navigationList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_navigationList->setSpacing(4);

    addNavigationItem(tr("Dashboard"), NavigationPage::Dashboard);
    addNavigationItem(tr("Realtime Tags"), NavigationPage::RealtimeTags);
    addNavigationItem(tr("Trend"), NavigationPage::Trend);
    addNavigationItem(tr("Alarm Center"), NavigationPage::AlarmCenter);
    addNavigationItem(tr("History"), NavigationPage::History);
    addNavigationItem(tr("Measurement Map"), NavigationPage::MeasurementMap);
    addNavigationItem(tr("Logs & Settings"), NavigationPage::LogsSettings);

    layout->addWidget(m_navigationList, 1);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->setSpacing(8);

    m_startButton = new QPushButton(tr("Start"), this);
    m_startButton->setObjectName(QStringLiteral("primaryButton"));
    m_stopButton = new QPushButton(tr("Stop"), this);
    m_stopButton->setObjectName(QStringLiteral("secondaryButton"));

    buttonLayout->addWidget(m_startButton);
    buttonLayout->addWidget(m_stopButton);
    layout->addLayout(buttonLayout);

    connect(m_navigationList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < m_pages.size()) {
            emit navigationRequested(pageForRow(row));
        }
    });
    connect(m_startButton, &QPushButton::clicked, this, &SideNavigationWidget::startRequested);
    connect(m_stopButton, &QPushButton::clicked, this, &SideNavigationWidget::stopRequested);
}

void SideNavigationWidget::setCurrentPage(NavigationPage page)
{
    const int row = m_pages.indexOf(page);
    if (row < 0 || !m_navigationList || m_navigationList->currentRow() == row) {
        return;
    }

    const QSignalBlocker blocker(m_navigationList);
    m_navigationList->setCurrentRow(row);
}

void SideNavigationWidget::addNavigationItem(const QString &title, NavigationPage page)
{
    auto *item = new QListWidgetItem(title, m_navigationList);
    item->setData(Qt::UserRole, static_cast<int>(page));
    item->setSizeHint(QSize(0, 40));
    m_pages.append(page);
}

NavigationPage SideNavigationWidget::pageForRow(int row) const
{
    if (row < 0 || row >= m_pages.size()) {
        return NavigationPage::Dashboard;
    }

    return m_pages.at(row);
}
