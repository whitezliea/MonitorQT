#ifndef SQLITEALARMREPOSITORY_H
#define SQLITEALARMREPOSITORY_H

#include "infrastructure/persistence/PersistenceModels.h"
#include "infrastructure/persistence/SqliteConnectionFactory.h"

namespace Monitor::Infrastructure::Persistence {

class SQLiteAlarmRepository
{
public:
    explicit SQLiteAlarmRepository(SqliteConnectionFactory *connectionFactory);

    void append(const QVector<Monitor::Domain::Alarms::AlarmEvent> &alarms);
    QVector<Monitor::Domain::Alarms::AlarmEvent> queryLatest(int count);
    QVector<Monitor::Domain::Alarms::AlarmEvent> queryOpenAlarms();
    AlarmQueryResult query(const AlarmQuery &query);

private:
    SqliteConnectionFactory *m_connectionFactory = nullptr;
};

} // namespace Monitor::Infrastructure::Persistence

#endif // SQLITEALARMREPOSITORY_H
