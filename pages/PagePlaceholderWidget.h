#ifndef PAGEPLACEHOLDERWIDGET_H
#define PAGEPLACEHOLDERWIDGET_H

#include "../navigation/NavigationPage.h"

#include <QWidget>

class PagePlaceholderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PagePlaceholderWidget(NavigationPage page, QWidget *parent = nullptr);
};

#endif // PAGEPLACEHOLDERWIDGET_H
