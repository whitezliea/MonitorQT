#include "PagePlaceholderWidget.h"

#include <QVBoxLayout>

PagePlaceholderWidget::PagePlaceholderWidget(NavigationPage page, QWidget *parent)
    : QWidget(parent)
{
    setObjectName(navigationPageObjectName(page));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *surface = new QWidget(this);
    surface->setObjectName(QStringLiteral("pageSurface"));
    layout->addWidget(surface);
}
