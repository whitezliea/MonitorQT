#include "EventBus.h"

#include <algorithm>

namespace Monitor::Application {

EventBus::EventBus(QObject *parent)
    : QObject(parent)
{
}

void EventBus::clearSubscriptions()
{
    m_subscriptions.clear();
}

bool EventBus::registerSubscription(const EventSubscriptionDescriptor &descriptor)
{
    if (!descriptor.enabled) {
        return true;
    }

    if (descriptor.eventName.trimmed().isEmpty() || descriptor.consumerName.trimmed().isEmpty()) {
        return false;
    }

    const auto duplicate = std::any_of(
        m_subscriptions.cbegin(),
        m_subscriptions.cend(),
        [&descriptor](const EventSubscriptionDescriptor &item) {
            return item.eventName == descriptor.eventName
                && item.consumerName == descriptor.consumerName;
        });
    if (duplicate) {
        return false;
    }

    m_subscriptions.append(descriptor);
    std::sort(
        m_subscriptions.begin(),
        m_subscriptions.end(),
        [](const EventSubscriptionDescriptor &left, const EventSubscriptionDescriptor &right) {
            return left.order < right.order;
        });

    emit subscriptionRegistered(descriptor.eventName, descriptor.consumerName);
    return true;
}

QVector<EventSubscriptionDescriptor> EventBus::subscriptions() const
{
    return m_subscriptions;
}

QStringList EventBus::validateSubscriptions() const
{
    QStringList errors;

    for (const auto &subscription : m_subscriptions) {
        if (subscription.eventName.trimmed().isEmpty()) {
            errors.append(QStringLiteral("Event subscription has an empty event name."));
        }

        if (subscription.consumerName.trimmed().isEmpty()) {
            errors.append(QStringLiteral("Event subscription has an empty consumer name."));
        }
    }

    return errors;
}

QString toString(EventHandlerFailurePolicy policy)
{
    switch (policy) {
    case EventHandlerFailurePolicy::Critical:
        return QStringLiteral("Critical");
    case EventHandlerFailurePolicy::Isolated:
        return QStringLiteral("Isolated");
    }

    return QString();
}

} // namespace Monitor::Application
