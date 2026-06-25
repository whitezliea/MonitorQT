#ifndef SQLITESCHEMAMIGRATIONS_H
#define SQLITESCHEMAMIGRATIONS_H

#include <QSqlDatabase>
#include <QString>
#include <QVector>

namespace Monitor::Infrastructure::Persistence {

struct SqliteSchemaMigration
{
    int version = 0;
    QString sql;
};

class SqliteSchemaMigrations
{
public:
    static QVector<SqliteSchemaMigration> all();
    static int currentVersion();
};

class SqliteSchemaMigrator
{
public:
    explicit SqliteSchemaMigrator(QVector<SqliteSchemaMigration> migrations = SqliteSchemaMigrations::all());

    int targetVersion() const;
    int migrate(QSqlDatabase &database) const;

private:
    static void validateMigrationDefinitions(const QVector<SqliteSchemaMigration> &migrations);
    static void ensureMigrationTable(QSqlDatabase &database);
    static QVector<int> appliedVersions(QSqlDatabase &database);
    void validateAppliedVersions(const QVector<int> &versions) const;
    static void applyMigration(QSqlDatabase &database, const SqliteSchemaMigration &migration);
    static void executeSqlBatch(QSqlDatabase &database, const QString &sql);

    QVector<SqliteSchemaMigration> m_migrations;
};

} // namespace Monitor::Infrastructure::Persistence

#endif // SQLITESCHEMAMIGRATIONS_H
