#include "EventBus.h"

#include <algorithm>
#include <exception>

namespace Monitor::Application {

EventBus::EventBus(QObject *parent)
    : QObject(parent)
{
}

void EventBus::clearSubscriptions()
{
    m_subscriptions.clear();
    m_handlers.clear();
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
            return sameSubscription(item, descriptor);
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

bool EventBus::registerHandler(const EventSubscriptionDescriptor &descriptor, EventHandler handler)
{
    if (!handler) {
        return false;
    }

    if (!descriptor.enabled) {
        return true;
    }

    if (descriptor.eventName.trimmed().isEmpty() || descriptor.consumerName.trimmed().isEmpty()) {
        return false;
    }

    const auto subscriptionExists = std::any_of(
        m_subscriptions.cbegin(),
        m_subscriptions.cend(),
        [&descriptor](const EventSubscriptionDescriptor &item) {
            return sameSubscription(item, descriptor);
        });
    if (!subscriptionExists && !registerSubscription(descriptor)) {
        return false;
    }

    const auto existingHandlers = m_handlers.constFind(descriptor.eventName);
    if (existingHandlers != m_handlers.cend()) {
        const auto duplicateHandler = std::any_of(
            existingHandlers.value().cbegin(),
            existingHandlers.value().cend(),
            [&descriptor](const HandlerRegistration &item) {
                return sameSubscription(item.descriptor, descriptor);
            });
        if (duplicateHandler) {
            return false;
        }
    }

    m_handlers[descriptor.eventName].append(HandlerRegistration{descriptor, std::move(handler)});
    std::sort(
        m_handlers[descriptor.eventName].begin(),
        m_handlers[descriptor.eventName].end(),
        [](const HandlerRegistration &left, const HandlerRegistration &right) {
            return left.descriptor.order < right.descriptor.order;
        });
    return true;
}

bool EventBus::registerHandler(
    const QString &eventName,
    const QString &consumerName,
    EventHandlerFailurePolicy failurePolicy,
    int order,
    EventHandler handler)
{
    EventSubscriptionDescriptor descriptor;
    descriptor.eventName = eventName;
    descriptor.consumerName = consumerName;
    descriptor.failurePolicy = failurePolicy;
    descriptor.order = order;
    return registerHandler(descriptor, std::move(handler));
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

bool EventBus::publish(
    const Monitor::Application::Events::ApplicationEvent &event,
    QStringList *errors) const
{
    QStringList localErrors;
    if (!errors) {
        errors = &localErrors;
    }

    const auto name = Monitor::Application::Events::eventName(event);
    const auto handlerIt = m_handlers.constFind(name);
    if (handlerIt == m_handlers.cend()) {
        return true;
    }

    for (const auto &registration : handlerIt.value()) {
        try {
            registration.handler(event);
        } catch (const std::exception &exception) {
            const auto message = QStringLiteral("Application event handler failed | EventType: %1 | Consumer: %2 | FailurePolicy: %3 | Error: %4")
                .arg(
                    name,
                    registration.descriptor.consumerName,
                    toString(registration.descriptor.failurePolicy),
                    QString::fromUtf8(exception.what()));
            errors->append(message);
            if (registration.descriptor.failurePolicy == EventHandlerFailurePolicy::Critical) {
                return false;
            }
        } catch (...) {
            const auto message = QStringLiteral("Application event handler failed | EventType: %1 | Consumer: %2 | FailurePolicy: %3 | Error: unknown")
                .arg(name, registration.descriptor.consumerName, toString(registration.descriptor.failurePolicy));
            errors->append(message);
            if (registration.descriptor.failurePolicy == EventHandlerFailurePolicy::Critical) {
                return false;
            }
        }
    }

    return true;
}

bool EventBus::sameSubscription(
    const EventSubscriptionDescriptor &left,
    const EventSubscriptionDescriptor &right)
{
    return left.eventName == right.eventName
        && left.consumerName == right.consumerName;
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
