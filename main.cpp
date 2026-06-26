#include "bootstrap/RuntimeComposition.h"
#include "infrastructure/persistence/SqliteConnectionFactory.h"
#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QFileInfo>
#include <QLocale>
#include <QTranslator>

#include <exception>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "MonitorQT_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    try {
        Monitor::Infrastructure::Persistence::SqliteConnectionFactory startupSqlite;
        startupSqlite.initialize();
        if (!QFileInfo::exists(startupSqlite.databasePath())) {
            qCritical().noquote() << "SQLite startup initialization failed: database file was not created at"
                                  << startupSqlite.databasePath();
            return 3;
        }
        if (startupSqlite.schemaVersion() !=
            Monitor::Infrastructure::Persistence::SqliteConnectionFactory::currentSchemaVersion()) {
            qCritical().noquote() << "SQLite startup initialization failed: schema version is not current.";
            return 3;
        }
    } catch (const std::exception &exception) {
        qCritical().noquote() << "SQLite startup initialization failed:"
                              << QString::fromUtf8(exception.what());
        return 3;
    }

    Monitor::Bootstrap::RuntimeComposition runtimeComposition;
    QStringList compositionErrors;
    if (!runtimeComposition.initialize(&compositionErrors)) {
        qCritical().noquote() << "Runtime composition initialization failed:"
                              << compositionErrors.join(QStringLiteral("; "));
        return 3;
    }

    QStringList hostErrors;
    if (!runtimeComposition.applicationRuntimeHost()->start(&hostErrors)) {
        qCritical().noquote() << "Application runtime host startup failed:"
                              << hostErrors.join(QStringLiteral("; "));
        return 3;
    }

    MainWindow w(
        runtimeComposition.runtimeCommandFacade(),
        runtimeComposition.runtimeUiSnapshotProvider(),
        runtimeComposition.historyQueryService(),
        runtimeComposition.alarmQueryService(),
        runtimeComposition.operationLogQueryService());
    w.show();
    const auto exitCode = QApplication::exec();

    QStringList shutdownErrors;
    if (!runtimeComposition.applicationRuntimeHost()->stop(&shutdownErrors)) {
        qWarning().noquote() << "Application runtime host shutdown completed with errors:"
                             << shutdownErrors.join(QStringLiteral("; "));
    }

    return exitCode;
}
