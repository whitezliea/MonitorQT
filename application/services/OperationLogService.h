#ifndef OPERATIONLOGSERVICE_H
#define OPERATIONLOGSERVICE_H

#include "application/queues/ApplicationQueues.h"
#include "domain/logs/LogModels.h"

#include <QDateTime>
#include <QString>

#include <optional>

namespace Monitor::Application::Services {

class OperationLogService
{
public:
    explicit OperationLogService(Monitor::Application::Queues::OperationLogQueue *queue);

    void write(
        Monitor::Domain::Logs::OperationLogLevel level,
        const QString &category,
        const QString &action,
        const QString &source,
        const QString &message,
        const std::optional<QString> &detail = std::nullopt,
        const std::optional<QString> &correlationId = std::nullopt,
        const QDateTime &timestampUtc = QDateTime::currentDateTimeUtc());

private:
    Monitor::Application::Queues::OperationLogQueue *m_queue = nullptr;
};

} // namespace Monitor::Application::Services

#endif // OPERATIONLOGSERVICE_H
