#ifndef EVENTBUS_H
#define EVENTBUS_H

#include "application/events/ApplicationEvents.h"

#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

namespace Monitor::Application {

enum class EventHandlerFailurePolicy {
    Critical,
    Isolated
};

struct EventSubscriptionDescriptor
{
    QString eventName;
    QString consumerName;
    EventHandlerFailurePolicy failurePolicy = EventHandlerFailurePolicy::Critical;
    int order = 0;
    bool enabled = true;
};

class EventBus : public QObject
{
    Q_OBJECT

public:
    using EventHandler = std::function<void(const Monitor::Application::Events::ApplicationEvent &)>;

    explicit EventBus(QObject *parent = nullptr);

    void clearSubscriptions();
    bool registerSubscription(const EventSubscriptionDescriptor &descriptor);
    bool registerHandler(const EventSubscriptionDescriptor &descriptor, EventHandler handler);
    bool registerHandler(
        const QString &eventName,
        const QString &consumerName,
        EventHandlerFailurePolicy failurePolicy,
        int order,
        EventHandler handler);
    QVector<EventSubscriptionDescriptor> subscriptions() const;
    QStringList validateSubscriptions() const;
    bool publish(
        const Monitor::Application::Events::ApplicationEvent &event,
        QStringList *errors = nullptr) const;

signals:
    void subscriptionRegistered(const QString &eventName, const QString &consumerName);

private:
    struct HandlerRegistration
    {
        EventSubscriptionDescriptor descriptor;
        EventHandler handler;
    };

    static bool sameSubscription(
        const EventSubscriptionDescriptor &left,
        const EventSubscriptionDescriptor &right);

    QVector<EventSubscriptionDescriptor> m_subscriptions;
    QHash<QString, QVector<HandlerRegistration>> m_handlers;
};

QString toString(EventHandlerFailurePolicy policy);

} // namespace Monitor::Application

#endif // EVENTBUS_H
