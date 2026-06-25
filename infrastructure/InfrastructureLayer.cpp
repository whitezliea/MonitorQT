#include "InfrastructureLayer.h"

#include "application/configuration/MonitorRuntimeOptions.h"
#include "domain/common/DomainCommon.h"
#include "infrastructure/persistence/HistoryRetentionService.h"
#include "infrastructure/persistence/SQLiteAlarmRepository.h"
#include "infrastructure/persistence/SQLiteConfigurationRepository.h"
#include "infrastructure/persistence/SQLiteHistoryRepository.h"
#include "infrastructure/persistence/SQLiteOperationLogRepository.h"
#include "infrastructure/persistence/SqliteConnectionFactory.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <exception>

namespace Monitor::Infrastructure {
namespace {

void addError(QStringList *errors, const QString &message)
{
    errors->append(message);
}

QDateTime validationTimestamp(int offsetMs = 0)
{
    const auto baseTicks = 638000000000000000LL;
    return Monitor::Domain::Common::UtcDateTime::fromCSharpTicks(
        baseTicks + static_cast<qint64>(offsetMs) * Monitor::Domain::Common::UtcDateTime::TicksPerMillisecond);
}

void removeSqliteFiles(const QString &databasePath)
{
    QFile::remove(databasePath);
    QFile::remove(databasePath + QStringLiteral("-wal"));
    QFile::remove(databasePath + QStringLiteral("-shm"));
}

bool nearlyEqual(double left, double right, double tolerance = 1e-6)
{
    return std::abs(left - right) <= tolerance;
}

bool containsLogAction(
    const QVector<Monitor::Domain::Logs::OperationLog> &logs,
    const QString &action)
{
    return std::any_of(logs.cbegin(), logs.cend(), [&action](const auto &log) {
        return log.action == action;
    });
}

} // namespace

InfrastructureLayerInfo infrastructureLayerInfo()
{
    return {
        QStringLiteral("MonitorInfrastructure"),
        {
            QStringLiteral("persistence"),
            QStringLiteral("logging"),
            QStringLiteral("export"),
            QStringLiteral("datasource"),
            QStringLiteral("system")
        },
        {
            QStringLiteral("MonitorApplication"),
            QStringLiteral("MonitorDomain")
        },
        {
            QStringLiteral("QtCore"),
            QStringLiteral("QtSql")
        }
    };
}

QStringList validateInfrastructureLayer()
{
    QStringList errors;

    const auto databasePath = QDir(QDir::tempPath()).filePath(
        QStringLiteral("MonitorQT_stage7_%1.db").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    removeSqliteFiles(databasePath);

    try {
        {
            using namespace Monitor::Infrastructure::Persistence;

            SqliteConnectionFactory factory(databasePath);
            factory.initialize();

            if (!QFileInfo::exists(databasePath)) {
                addError(&errors, QStringLiteral("SqliteConnectionFactory must create the database file on first initialization."));
            }
            if (factory.schemaVersion() != SqliteConnectionFactory::currentSchemaVersion()) {
                addError(&errors, QStringLiteral("SQLite schema version must match the current migration version."));
            }
            if (factory.journalMode().compare(QStringLiteral("wal"), Qt::CaseInsensitive) != 0) {
                addError(&errors, QStringLiteral("SQLite journal mode must be WAL."));
            }

            {
                auto connection = factory.openReadConnection();
                QSqlQuery query(connection.database());
                if (!query.exec(QStringLiteral(R"SQL(
                    SELECT COUNT(*)
                    FROM sqlite_master
                    WHERE type = 'table'
                      AND name IN ('history_samples', 'alarm_events', 'operation_logs',
                                   'tag_runtime_settings', 'runtime_settings', 'schema_migrations');
                )SQL")) || !query.next() || query.value(0).toInt() != 6) {
                    addError(&errors, QStringLiteral("SQLite migration must create all stage-7 persistence tables."));
                }
            }

            SQLiteHistoryRepository historyRepository(&factory);
            SQLiteAlarmRepository alarmRepository(&factory);
            SQLiteOperationLogRepository operationLogRepository(&factory);
            SQLiteConfigurationRepository configurationRepository(&factory);

            const auto now = validationTimestamp();
            historyRepository.append({
                Monitor::Domain::Tags::TagValue{
                    QStringLiteral("TAG.A"),
                    10.0,
                    now,
                    Monitor::Domain::Tags::TagQuality::Good,
                    Monitor::Domain::Tags::TagAlarmState::Normal,
                    QStringLiteral("frame-1"),
                    1
                },
                Monitor::Domain::Tags::TagValue{
                    QStringLiteral("TAG.A"),
                    11.0,
                    now.addSecs(1),
                    Monitor::Domain::Tags::TagQuality::Good,
                    Monitor::Domain::Tags::TagAlarmState::WarningHigh,
                    QStringLiteral("frame-2"),
                    2
                }
            });

            const auto historyResult = historyRepository.query(HistoryQuery{
                QStringLiteral("TAG.A"),
                now.addSecs(-1),
                now.addSecs(2),
                1,
                10,
                HistorySortDirection::Ascending
            });
            if (historyResult.items.size() != 2 ||
                historyResult.totalCount != 2 ||
                !nearlyEqual(historyResult.items.at(0).value, 10.0) ||
                historyResult.items.at(1).alarmState != Monitor::Domain::Tags::TagAlarmState::WarningHigh) {
                addError(&errors, QStringLiteral("SQLiteHistoryRepository must persist and query samples with paging metadata."));
            }

            const auto duplicateSource = QStringLiteral("dedup-source");
            const auto duplicateSample = Monitor::Domain::Tags::TagValue{
                QStringLiteral("TAG.DEDUP"),
                1.0,
                now,
                Monitor::Domain::Tags::TagQuality::Good,
                Monitor::Domain::Tags::TagAlarmState::Normal,
                duplicateSource,
                1
            };
            historyRepository.append({duplicateSample});
            historyRepository.append({duplicateSample});
            const auto duplicateResult = historyRepository.query(HistoryQuery{
                QStringLiteral("TAG.DEDUP"),
                now.addSecs(-1),
                now.addSecs(1),
                1,
                10,
                HistorySortDirection::Ascending
            });
            if (duplicateResult.items.size() != 1) {
                addError(&errors, QStringLiteral("SQLiteHistoryRepository must ignore duplicate tag/source history samples."));
            }

            const auto alarmId = QUuid::createUuid();
            Monitor::Domain::Alarms::AlarmEvent alarm;
            alarm.alarmId = alarmId;
            alarm.tagId = QStringLiteral("TAG.ALARM");
            alarm.level = Monitor::Domain::Alarms::AlarmLevel::Alarm;
            alarm.state = Monitor::Domain::Alarms::AlarmState::Active;
            alarm.triggerValue = 25.0;
            alarm.triggerTimeUtc = now;
            alarm.message = QStringLiteral("Raised");
            alarm.alarmType = Monitor::Domain::Tags::TagAlarmState::AlarmHigh;
            alarmRepository.append({alarm});
            if (alarmRepository.queryOpenAlarms().size() != 1) {
                addError(&errors, QStringLiteral("SQLiteAlarmRepository must query active open alarms."));
            }

            alarm.state = Monitor::Domain::Alarms::AlarmState::Recovered;
            alarm.message = QStringLiteral("Recovered");
            alarm.acknowledgeTimeUtc = now.addSecs(1);
            alarm.recoverTimeUtc = now.addSecs(2);
            alarm.lastUpdatedTimeUtc = now.addSecs(2);
            alarm.closeReason = QStringLiteral("RecoveredToNormal");
            alarmRepository.append({alarm});
            const auto latestAlarms = alarmRepository.queryLatest(10);
            const auto filteredAlarms = alarmRepository.query(AlarmQuery{
                now.addSecs(-1),
                now.addSecs(3),
                QStringLiteral("TAG.ALARM"),
                Monitor::Domain::Alarms::AlarmLevel::Alarm,
                Monitor::Domain::Alarms::AlarmState::Recovered,
                1,
                10,
                AlarmSortDirection::Descending
            });
            if (latestAlarms.size() != 1 ||
                latestAlarms.first().state != Monitor::Domain::Alarms::AlarmState::Recovered ||
                filteredAlarms.items.size() != 1 ||
                filteredAlarms.items.first().alarmId != alarmId) {
                addError(&errors, QStringLiteral("SQLiteAlarmRepository must upsert alarm lifecycle and support paged filters."));
            }

            operationLogRepository.append({
                Monitor::Domain::Logs::OperationLog{
                    now,
                    Monitor::Domain::Logs::OperationLogLevel::Info,
                    QStringLiteral("Acquisition"),
                    QStringLiteral("Started"),
                    QStringLiteral("Acquisition.Started"),
                    QStringLiteral("validation"),
                    QStringLiteral("detail-a"),
                    QStringLiteral("correlation-a"),
                    0
                },
                Monitor::Domain::Logs::OperationLog{
                    now.addSecs(1),
                    Monitor::Domain::Logs::OperationLogLevel::Warning,
                    QStringLiteral("Alarm"),
                    QStringLiteral("Acknowledged"),
                    QStringLiteral("Alarm.Acknowledged"),
                    QStringLiteral("validation"),
                    std::nullopt,
                    QStringLiteral("correlation-b"),
                    0
                }
            });
            const auto warningLogs = operationLogRepository.query(Monitor::Domain::Logs::OperationLogQuery{
                now.addSecs(-1),
                now.addSecs(2),
                Monitor::Domain::Logs::OperationLogLevel::Warning,
                QStringLiteral("alarm"),
                20
            });
            if (warningLogs.size() != 1 ||
                warningLogs.first().id <= 0 ||
                warningLogs.first().action != QStringLiteral("Alarm.Acknowledged") ||
                !warningLogs.first().correlationId.has_value()) {
                addError(&errors, QStringLiteral("SQLiteOperationLogRepository must persist and filter logs across categories."));
            }

            configurationRepository.saveTagConfigurations({
                Monitor::Application::Configuration::TagRuntimeConfiguration{
                    QStringLiteral("TEST.TAG"),
                    false,
                    2.0,
                    1.0,
                    8.0,
                    9.0,
                    true,
                    2500,
                    0
                }
            });
            configurationRepository.saveRuntimeSettings({
                {Monitor::Application::Configuration::RuntimeSettingKeys::UiRefreshIntervalMs, QStringLiteral("250")},
                {Monitor::Application::Configuration::RuntimeSettingKeys::DataGenerateIntervalMs, QStringLiteral("750")}
            });
            const auto tagConfigurations = configurationRepository.loadTagConfigurations();
            const auto runtimeSettings = configurationRepository.loadRuntimeSettings();
            if (tagConfigurations.size() != 1 ||
                tagConfigurations.first().tagId != QStringLiteral("TEST.TAG") ||
                tagConfigurations.first().alarmEnabled ||
                tagConfigurations.first().historyIntervalMs != 2500 ||
                runtimeSettings.value(Monitor::Application::Configuration::RuntimeSettingKeys::UiRefreshIntervalMs) != QStringLiteral("250") ||
                runtimeSettings.value(Monitor::Application::Configuration::RuntimeSettingKeys::DataGenerateIntervalMs) != QStringLiteral("750")) {
                addError(&errors, QStringLiteral("SQLiteConfigurationRepository must persist tag and runtime settings."));
            }

            historyRepository.append({
                Monitor::Domain::Tags::TagValue{
                    QStringLiteral("TAG.OLD"),
                    99.0,
                    now.addDays(-40),
                    Monitor::Domain::Tags::TagQuality::Good,
                    Monitor::Domain::Tags::TagAlarmState::Normal,
                    QStringLiteral("old-frame"),
                    9
                }
            });
            HistoryRetentionService retentionService(
                &historyRepository,
                &operationLogRepository,
                30,
                1);
            const auto retention = retentionService.cleanup(now);
            const auto logsAfterRetention = operationLogRepository.queryLatest(20);
            if (retention.deletedCount < 1 ||
                retention.cutoffUtc != now.addDays(-30) ||
                !containsLogAction(logsAfterRetention, QStringLiteral("History.RetentionCleanup"))) {
                addError(&errors, QStringLiteral("HistoryRetentionService must delete old history in batches and write an operation log."));
            }
        }
    } catch (const std::exception &exception) {
        addError(&errors, QStringLiteral("Infrastructure validation threw unexpectedly: %1").arg(QString::fromUtf8(exception.what())));
    }

    removeSqliteFiles(databasePath);
    return errors;
}

} // namespace Monitor::Infrastructure
