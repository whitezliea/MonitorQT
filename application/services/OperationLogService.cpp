#include "OperationLogService.h"

#include "domain/common/DomainCommon.h"

#include <stdexcept>

namespace Monitor::Application::Services {

OperationLogService::OperationLogService(Monitor::Application::Queues::OperationLogQueue *queue)
    : m_queue(queue)
{
    if (!m_queue) {
        throw std::invalid_argument("OperationLogQueue must not be null.");
    }
}

void OperationLogService::write(
    Monitor::Domain::Logs::OperationLogLevel level,
    const QString &category,
    const QString &action,
    const QString &source,
    const QString &message,
    const std::optional<QString> &detail,
    const std::optional<QString> &correlationId,
    const QDateTime &timestampUtc)
{
    const auto utc = Monitor::Domain::Common::UtcDateTime::require(timestampUtc, QStringLiteral("timestampUtc"));
    m_queue->enqueue(Monitor::Domain::Logs::OperationLog{
        utc,
        level,
        category,
        message,
        action,
        source,
        detail,
        correlationId,
        0
    });
}

} // namespace Monitor::Application::Services
