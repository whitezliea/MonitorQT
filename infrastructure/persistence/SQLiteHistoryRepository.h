#ifndef SQLITEHISTORYREPOSITORY_H
#define SQLITEHISTORYREPOSITORY_H

#include "infrastructure/persistence/PersistenceModels.h"
#include "infrastructure/persistence/SqliteConnectionFactory.h"

namespace Monitor::Infrastructure::Persistence {

class SQLiteHistoryRepository
{
public:
    explicit SQLiteHistoryRepository(SqliteConnectionFactory *connectionFactory);

    void append(const QVector<Monitor::Domain::Tags::TagValue> &samples);
    HistoryQueryResult query(const HistoryQuery &query);
    QVector<Monitor::Domain::Tags::TagValue> query(
        const QString &tagId,
        const QDateTime &startTimeUtc,
        const QDateTime &endTimeUtc);
    int deleteBefore(const QDateTime &cutoffUtc, int maxRows);

private:
    SqliteConnectionFactory *m_connectionFactory = nullptr;
};

} // namespace Monitor::Infrastructure::Persistence

#endif // SQLITEHISTORYREPOSITORY_H
