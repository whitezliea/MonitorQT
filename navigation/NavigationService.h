#ifndef NAVIGATIONSERVICE_H
#define NAVIGATIONSERVICE_H

#include "NavigationPage.h"

#include <QMap>
#include <QObject>

class QStackedWidget;
class QWidget;

class NavigationService : public QObject
{
    Q_OBJECT

public:
    explicit NavigationService(QStackedWidget *stack, QObject *parent = nullptr);

    void registerPage(NavigationPage page, QWidget *widget);
    NavigationPage currentPage() const;

public slots:
    void navigateTo(NavigationPage page);

signals:
    void currentPageChanged(NavigationPage page, const QString &title);

private:
    QStackedWidget *m_stack = nullptr;
    QMap<NavigationPage, int> m_pageIndexes;
    NavigationPage m_currentPage = NavigationPage::Dashboard;
    bool m_hasCurrentPage = false;
};

#endif // NAVIGATIONSERVICE_H
