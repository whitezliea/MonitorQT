#ifndef TOPSTATUSBARWIDGET_H
#define TOPSTATUSBARWIDGET_H

#include <QWidget>

class QLabel;

class TopStatusBarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TopStatusBarWidget(QWidget *parent = nullptr);

    void setDeviceId(const QString &deviceId);
    void setRunning(bool running);
    void setAcquisitionConnected(bool connected);

private:
    void setBadge(QLabel *label, const QString &text, const QString &backgroundColor, const QString &textColor);

    QLabel *m_deviceLabel = nullptr;
    QLabel *m_connectionBadge = nullptr;
    QLabel *m_runningBadge = nullptr;
};

#endif // TOPSTATUSBARWIDGET_H
