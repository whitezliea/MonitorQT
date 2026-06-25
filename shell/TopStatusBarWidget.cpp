#include "TopStatusBarWidget.h"

#include <QHBoxLayout>
#include <QLabel>

TopStatusBarWidget::TopStatusBarWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("topStatusBar"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(20, 10, 20, 10);
    layout->setSpacing(12);

    auto *titleLabel = new QLabel(tr("MultiChannel Monitor"), this);
    titleLabel->setObjectName(QStringLiteral("appTitleLabel"));

    m_deviceLabel = new QLabel(this);
    m_connectionBadge = new QLabel(this);
    m_runningBadge = new QLabel(this);

    layout->addWidget(titleLabel);
    layout->addStretch(1);
    layout->addWidget(m_deviceLabel);
    layout->addWidget(m_connectionBadge);
    layout->addWidget(m_runningBadge);

    setDeviceId(QStringLiteral("MCMD-001"));
    setAcquisitionConnected(false);
    setRunning(false);
}

void TopStatusBarWidget::setDeviceId(const QString &deviceId)
{
    m_deviceLabel->setText(tr("Device: %1").arg(deviceId));
}

void TopStatusBarWidget::setRunning(bool running)
{
    if (running) {
        setBadge(m_runningBadge, tr("Running"), QStringLiteral("#DDF7E8"), QStringLiteral("#166534"));
    } else {
        setBadge(m_runningBadge, tr("Stopped"), QStringLiteral("#E5E7EB"), QStringLiteral("#374151"));
    }
}

void TopStatusBarWidget::setAcquisitionConnected(bool connected)
{
    if (connected) {
        setBadge(m_connectionBadge, tr("Connected"), QStringLiteral("#E0F2FE"), QStringLiteral("#075985"));
    } else {
        setBadge(m_connectionBadge, tr("Disconnected"), QStringLiteral("#FEE2E2"), QStringLiteral("#991B1B"));
    }
}

void TopStatusBarWidget::setBadge(QLabel *label,
                                  const QString &text,
                                  const QString &backgroundColor,
                                  const QString &textColor)
{
    if (!label) {
        return;
    }

    label->setText(text);
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumWidth(92);
    label->setStyleSheet(QStringLiteral(
                             "QLabel {"
                             "  background: %1;"
                             "  color: %2;"
                             "  border-radius: 10px;"
                             "  padding: 4px 10px;"
                             "  font-weight: 600;"
                             "}")
                             .arg(backgroundColor, textColor));
}
