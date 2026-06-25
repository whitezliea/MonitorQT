#include "bootstrap/RuntimeComposition.h"
#include "mainwindow.h"

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

    Monitor::Bootstrap::RuntimeComposition runtimeComposition;
    QStringList compositionErrors;
    if (!runtimeComposition.initialize(&compositionErrors)) {
        qCritical().noquote() << "Runtime composition initialization failed:"
                              << compositionErrors.join(QStringLiteral("; "));
        return 3;
    }

    MainWindow w;
    w.show();
    return QApplication::exec();
}
