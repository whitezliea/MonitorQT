#ifndef EVENTBUS_H
#define EVENTBUS_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

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
    explicit EventBus(QObject *parent = nullptr);

    void clearSubscriptions();
    bool registerSubscription(const EventSubscriptionDescriptor &descriptor);
    QVector<EventSubscriptionDescriptor> subscriptions() const;
    QStringList validateSubscriptions() const;

signals:
    void subscriptionRegistered(const QString &eventName, const QString &consumerName);

private:
    QVector<EventSubscriptionDescriptor> m_subscriptions;
};

QString toString(EventHandlerFailurePolicy policy);

} // namespace Monitor::Application

#endif // EVENTBUS_H
