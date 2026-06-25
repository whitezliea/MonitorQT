#ifndef HISTORYRETENTIONSERVICE_H
#define HISTORYRETENTIONSERVICE_H

#include "infrastructure/persistence/PersistenceModels.h"
#include "infrastructure/persistence/SQLiteHistoryRepository.h"
#include "infrastructure/persistence/SQLiteOperationLogRepository.h"

namespace Monitor::Infrastructure::Persistence {

class HistoryRetentionService
{
public:
    HistoryRetentionService(
        SQLiteHistoryRepository *historyRepository,
        SQLiteOperationLogRepository *operationLogRepository,
        int retentionDays,
        int deleteBatchSize = 1000);

    HistoryRetentionResult cleanup(
        const QDateTime &nowUtc = QDateTime::currentDateTimeUtc());

private:
    SQLiteHistoryRepository *m_historyRepository = nullptr;
    SQLiteOperationLogRepository *m_operationLogRepository = nullptr;
    int m_retentionDays = 0;
    int m_deleteBatchSize = 0;
};

} // namespace Monitor::Infrastructure::Persistence

#endif // HISTORYRETENTIONSERVICE_H
