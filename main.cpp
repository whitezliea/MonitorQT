#include "mainwindow.h"
#include "phase0/SourceBehaviorFreeze.h"

#include <QApplication>
#include <QDebug>
#include <QLocale>
#include <QTranslator>

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

    QStringList phase0Errors;
    if (!Phase0::validateSourceBehaviorFreeze(&phase0Errors)) {
        qCritical().noquote() << "Phase 0 source behavior freeze validation failed:"
                              << phase0Errors.join(QStringLiteral("; "));
        return 2;
    }

    MainWindow w;
    w.show();
    return QApplication::exec();
}
