#ifndef SIDENAVIGATIONWIDGET_H
#define SIDENAVIGATIONWIDGET_H

#include "../navigation/NavigationPage.h"

#include <QVector>
#include <QWidget>

class QListWidget;
class QPushButton;

class SideNavigationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SideNavigationWidget(QWidget *parent = nullptr);

    void setCurrentPage(NavigationPage page);

signals:
    void navigationRequested(NavigationPage page);
    void startRequested();
    void stopRequested();

private:
    void addNavigationItem(const QString &title, NavigationPage page);
    NavigationPage pageForRow(int row) const;

    QListWidget *m_navigationList = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QVector<NavigationPage> m_pages;
};

#endif // SIDENAVIGATIONWIDGET_H
