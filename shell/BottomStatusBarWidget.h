#ifndef BOTTOMSTATUSBARWIDGET_H
#define BOTTOMSTATUSBARWIDGET_H

#include <QDateTime>
#include <QWidget>

class QLabel;

class BottomStatusBarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BottomStatusBarWidget(QWidget *parent = nullptr);

    void setDataSourceConnected(bool connected);
    void setDatabaseConnected(bool connected);
    void setLastFrame(quint64 frameIndex);
    void setMatrixFrame(qint64 frameIndex);
    void setSyncState(const QString &state);
    void setRefreshIntervalMs(int intervalMs);
    void setCurrentTime(const QDateTime &dateTime);

private:
    QLabel *m_dataSourceLabel = nullptr;
    QLabel *m_databaseLabel = nullptr;
    QLabel *m_lastFrameLabel = nullptr;
    QLabel *m_matrixFrameLabel = nullptr;
    QLabel *m_syncStateLabel = nullptr;
    QLabel *m_refreshLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
};

#endif // BOTTOMSTATUSBARWIDGET_H
