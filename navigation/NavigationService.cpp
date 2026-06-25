#include "NavigationService.h"

#include <QStackedWidget>
#include <QWidget>

NavigationService::NavigationService(QStackedWidget *stack, QObject *parent)
    : QObject(parent)
    , m_stack(stack)
{
}

void NavigationService::registerPage(NavigationPage page, QWidget *widget)
{
    if (!m_stack || !widget || m_pageIndexes.contains(page)) {
        return;
    }

    const int index = m_stack->addWidget(widget);
    m_pageIndexes.insert(page, index);
}

NavigationPage NavigationService::currentPage() const
{
    return m_currentPage;
}

void NavigationService::navigateTo(NavigationPage page)
{
    if (!m_stack || !m_pageIndexes.contains(page)) {
        return;
    }

    const int index = m_pageIndexes.value(page);
    if (m_stack->currentIndex() != index) {
        m_stack->setCurrentIndex(index);
    }

    if (!m_hasCurrentPage || m_currentPage != page) {
        m_currentPage = page;
        m_hasCurrentPage = true;
        emit currentPageChanged(page, navigationPageTitle(page));
    }
}
