#include "BottomStatusBarWidget.h"

#include <QHBoxLayout>
#include <QLabel>

BottomStatusBarWidget::BottomStatusBarWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("bottomStatusBar"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(20, 8, 20, 8);
    layout->setSpacing(16);

    m_dataSourceLabel = new QLabel(this);
    m_lastFrameLabel = new QLabel(this);
    m_refreshLabel = new QLabel(this);
    m_timeLabel = new QLabel(this);

    layout->addWidget(m_dataSourceLabel);
    layout->addWidget(m_lastFrameLabel);
    layout->addWidget(m_refreshLabel);
    layout->addStretch(1);
    layout->addWidget(m_timeLabel);

    setDataSourceConnected(false);
    setLastFrame(0);
    setRefreshIntervalMs(1000);
    setCurrentTime(QDateTime::currentDateTime());
}

void BottomStatusBarWidget::setDataSourceConnected(bool connected)
{
    m_dataSourceLabel->setText(tr("DataSource: %1").arg(connected ? tr("Connected") : tr("Disconnected")));
}

void BottomStatusBarWidget::setLastFrame(quint64 frameIndex)
{
    m_lastFrameLabel->setText(tr("Last Frame: %1").arg(frameIndex));
}

void BottomStatusBarWidget::setRefreshIntervalMs(int intervalMs)
{
    m_refreshLabel->setText(tr("UI Refresh: %1 ms").arg(intervalMs));
}

void BottomStatusBarWidget::setCurrentTime(const QDateTime &dateTime)
{
    m_timeLabel->setText(dateTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
}
