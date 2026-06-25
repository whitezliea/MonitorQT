#include "EventRegistration.h"

namespace Monitor::Bootstrap {
namespace {

using Monitor::Application::EventHandlerFailurePolicy;
using Monitor::Application::EventSubscriptionDescriptor;

EventSubscriptionDescriptor subscription(
    const QString &eventName,
    const QString &consumerName,
    EventHandlerFailurePolicy failurePolicy,
    int order)
{
    EventSubscriptionDescriptor descriptor;
    descriptor.eventName = eventName;
    descriptor.consumerName = consumerName;
    descriptor.failurePolicy = failurePolicy;
    descriptor.order = order;
    return descriptor;
}

void appendError(QStringList *errors, const QString &message)
{
    if (errors) {
        errors->append(message);
    }
}

} // namespace

QVector<EventSubscriptionDescriptor> EventRegistration::defaultSubscriptions()
{
    return {
        subscription(QStringLiteral("RawFrameReceivedEvent"),
                     QStringLiteral("MeasurementMapFrameConsumer"),
                     EventHandlerFailurePolicy::Isolated,
                     10),
        subscription(QStringLiteral("TagRuntimeStatesProducedEvent"),
                     QStringLiteral("TagCacheConsumer"),
                     EventHandlerFailurePolicy::Critical,
                     20),
        subscription(QStringLiteral("DataSourceTimedOutEvent"),
                     QStringLiteral("DataSourceHealthOperationLogConsumer"),
                     EventHandlerFailurePolicy::Isolated,
                     30),
        subscription(QStringLiteral("DataSourceRecoveredEvent"),
                     QStringLiteral("DataSourceHealthOperationLogConsumer"),
                     EventHandlerFailurePolicy::Isolated,
                     40),
        subscription(QStringLiteral("TagRuntimeStatesProducedEvent"),
                     QStringLiteral("HistoryRuntimeStateConsumer"),
                     EventHandlerFailurePolicy::Isolated,
                     50),
        subscription(QStringLiteral("AlarmRaisedEvent"),
                     QStringLiteral("AlarmEventConsumer"),
                     EventHandlerFailurePolicy::Isolated,
                     60),
        subscription(QStringLiteral("AlarmUpdatedEvent"),
                     QStringLiteral("AlarmEventConsumer"),
                     EventHandlerFailurePolicy::Isolated,
                     70),
        subscription(QStringLiteral("AlarmRecoveredEvent"),
                     QStringLiteral("AlarmEventConsumer"),
                     EventHandlerFailurePolicy::Isolated,
                     80),
        subscription(QStringLiteral("AlarmAcknowledgedEvent"),
                     QStringLiteral("AlarmEventConsumer"),
                     EventHandlerFailurePolicy::Isolated,
                     90),
        subscription(QStringLiteral("AlarmRaisedEvent"),
                     QStringLiteral("AlarmOperationLogConsumer"),
                     EventHandlerFailurePolicy::Isolated,
                     100),
        subscription(QStringLiteral("AlarmUpdatedEvent"),
                     QStringLiteral("AlarmOperationLogConsumer"),
                     EventHandlerFailurePolicy::Isolated,
                     110),
        subscription(QStringLiteral("AlarmRecoveredEvent"),
                     QStringLiteral("AlarmOperationLogConsumer"),
                     EventHandlerFailurePolicy::Isolated,
                     120),
        subscription(QStringLiteral("AlarmAcknowledgedEvent"),
                     QStringLiteral("AlarmOperationLogConsumer"),
                     EventHandlerFailurePolicy::Isolated,
                     130)
    };
}

bool EventRegistration::registerApplicationEvents(
    Monitor::Application::EventBus &eventBus,
    QStringList *errors)
{
    eventBus.clearSubscriptions();
    auto success = true;

    for (const auto &descriptor : defaultSubscriptions()) {
        if (!eventBus.registerSubscription(descriptor)) {
            success = false;
            appendError(
                errors,
                QStringLiteral("Failed to register %1 -> %2.")
                    .arg(descriptor.eventName, descriptor.consumerName));
        }
    }

    const auto validationErrors = eventBus.validateSubscriptions();
    if (!validationErrors.isEmpty() && errors) {
        errors->append(validationErrors);
    }

    return success && validationErrors.isEmpty() && (!errors || errors->isEmpty());
}

} // namespace Monitor::Bootstrap
