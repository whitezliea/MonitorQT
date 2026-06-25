#include "HistoryRetentionService.h"

#include "domain/common/DomainCommon.h"

#include <QString>

#include <stdexcept>

namespace Monitor::Infrastructure::Persistence {

HistoryRetentionService::HistoryRetentionService(
    SQLiteHistoryRepository *historyRepository,
    SQLiteOperationLogRepository *operationLogRepository,
    int retentionDays,
    int deleteBatchSize)
    : m_historyRepository(historyRepository),
      m_operationLogRepository(operationLogRepository),
      m_retentionDays(retentionDays),
      m_deleteBatchSize(deleteBatchSize)
{
    if (!m_historyRepository) {
        throw std::invalid_argument("SQLiteHistoryRepository must not be null.");
    }
    if (!m_operationLogRepository) {
        throw std::invalid_argument("SQLiteOperationLogRepository must not be null.");
    }
    if (m_retentionDays <= 0) {
        throw std::out_of_range("retentionDays must be greater than zero.");
    }
    if (m_deleteBatchSize <= 0) {
        throw std::out_of_range("deleteBatchSize must be greater than zero.");
    }
}

HistoryRetentionResult HistoryRetentionService::cleanup(const QDateTime &nowUtc)
{
    const auto utcNow = Monitor::Domain::Common::UtcDateTime::require(nowUtc, QStringLiteral("nowUtc"));
    const auto cutoffUtc = utcNow.addDays(-m_retentionDays);

    qint64 totalDeleted = 0;
    while (true) {
        const auto deleted = m_historyRepository->deleteBefore(cutoffUtc, m_deleteBatchSize);
        totalDeleted += deleted;
        if (deleted < m_deleteBatchSize) {
            break;
        }
    }

    m_operationLogRepository->append({
        Monitor::Domain::Logs::OperationLog{
            utcNow,
            Monitor::Domain::Logs::OperationLogLevel::Info,
            QStringLiteral("History"),
            QStringLiteral("History retention cleanup completed."),
            QStringLiteral("History.RetentionCleanup"),
            QStringLiteral("HistoryRetentionService"),
            QStringLiteral("CutoffUtc=%1; Deleted=%2; RetentionDays=%3")
                .arg(cutoffUtc.toString(Qt::ISODateWithMs))
                .arg(totalDeleted)
                .arg(m_retentionDays),
            std::nullopt,
            0
        }
    });

    return {totalDeleted, cutoffUtc};
}

} // namespace Monitor::Infrastructure::Persistence
