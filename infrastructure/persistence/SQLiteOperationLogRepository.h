#ifndef SQLITEOPERATIONLOGREPOSITORY_H
#define SQLITEOPERATIONLOGREPOSITORY_H

#include "domain/logs/LogModels.h"
#include "infrastructure/persistence/SqliteConnectionFactory.h"

#include <QVector>

namespace Monitor::Infrastructure::Persistence {

class SQLiteOperationLogRepository
{
public:
    explicit SQLiteOperationLogRepository(SqliteConnectionFactory *connectionFactory);

    void append(const QVector<Monitor::Domain::Logs::OperationLog> &logs);
    QVector<Monitor::Domain::Logs::OperationLog> queryLatest(int count);
    QVector<Monitor::Domain::Logs::OperationLog> query(
        const Monitor::Domain::Logs::OperationLogQuery &query);
    Monitor::Domain::Logs::OperationLogQueryResult queryPage(
        const Monitor::Domain::Logs::OperationLogQuery &query);

private:
    SqliteConnectionFactory *m_connectionFactory = nullptr;
};

} // namespace Monitor::Infrastructure::Persistence

#endif // SQLITEOPERATIONLOGREPOSITORY_H
