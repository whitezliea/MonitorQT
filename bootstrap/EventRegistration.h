#ifndef EVENTREGISTRATION_H
#define EVENTREGISTRATION_H

#include "application/events/EventBus.h"

#include <QStringList>
#include <QVector>

namespace Monitor::Bootstrap {

class EventRegistration
{
public:
    static QVector<Monitor::Application::EventSubscriptionDescriptor> defaultSubscriptions();
    static bool registerApplicationEvents(
        Monitor::Application::EventBus &eventBus,
        QStringList *errors = nullptr);
};

} // namespace Monitor::Bootstrap

#endif // EVENTREGISTRATION_H
