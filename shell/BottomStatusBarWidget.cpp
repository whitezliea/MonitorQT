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
    m_databaseLabel = new QLabel(this);
    m_lastFrameLabel = new QLabel(this);
    m_matrixFrameLabel = new QLabel(this);
    m_syncStateLabel = new QLabel(this);
    m_refreshLabel = new QLabel(this);
    m_timeLabel = new QLabel(this);

    layout->addWidget(m_dataSourceLabel);
    layout->addWidget(m_databaseLabel);
    layout->addWidget(m_lastFrameLabel);
    layout->addWidget(m_matrixFrameLabel);
    layout->addWidget(m_syncStateLabel);
    layout->addWidget(m_refreshLabel);
    layout->addStretch(1);
    layout->addWidget(m_timeLabel);

    setDataSourceConnected(false);
    setDatabaseConnected(false);
    setLastFrame(0);
    setMatrixFrame(0);
    setSyncState(QStringLiteral("Idle"));
    setRefreshIntervalMs(1000);
    setCurrentTime(QDateTime::currentDateTime());
}

void BottomStatusBarWidget::setDataSourceConnected(bool connected)
{
    m_dataSourceLabel->setText(tr("DataSource: %1").arg(connected ? tr("Connected") : tr("Disconnected")));
}

void BottomStatusBarWidget::setDatabaseConnected(bool connected)
{
    m_databaseLabel->setText(tr("DB: %1").arg(connected ? tr("Ready") : tr("Offline")));
}

void BottomStatusBarWidget::setLastFrame(quint64 frameIndex)
{
    m_lastFrameLabel->setText(tr("Last Frame: %1").arg(frameIndex));
}

void BottomStatusBarWidget::setMatrixFrame(qint64 frameIndex)
{
    m_matrixFrameLabel->setText(tr("Matrix Frame: %1").arg(frameIndex));
}

void BottomStatusBarWidget::setSyncState(const QString &state)
{
    m_syncStateLabel->setText(tr("Sync: %1").arg(state));
}

void BottomStatusBarWidget::setRefreshIntervalMs(int intervalMs)
{
    m_refreshLabel->setText(tr("UI Refresh: %1 ms").arg(intervalMs));
}

void BottomStatusBarWidget::setCurrentTime(const QDateTime &dateTime)
{
    m_timeLabel->setText(dateTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
}
