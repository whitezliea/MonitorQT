#ifndef NAVIGATIONPAGE_H
#define NAVIGATIONPAGE_H

#include <QMetaType>
#include <QString>

enum class NavigationPage {
    Dashboard = 0,
    RealtimeTags,
    Trend,
    AlarmCenter,
    History,
    MeasurementMap,
    LogsSettings
};

inline QString navigationPageTitle(NavigationPage page)
{
    switch (page) {
    case NavigationPage::Dashboard:
        return QStringLiteral("Dashboard");
    case NavigationPage::RealtimeTags:
        return QStringLiteral("Realtime Tags");
    case NavigationPage::Trend:
        return QStringLiteral("Trend");
    case NavigationPage::AlarmCenter:
        return QStringLiteral("Alarm Center");
    case NavigationPage::History:
        return QStringLiteral("History");
    case NavigationPage::MeasurementMap:
        return QStringLiteral("Measurement Map");
    case NavigationPage::LogsSettings:
        return QStringLiteral("Logs & Settings");
    }

    return QString();
}

inline QString navigationPageObjectName(NavigationPage page)
{
    switch (page) {
    case NavigationPage::Dashboard:
        return QStringLiteral("dashboardPage");
    case NavigationPage::RealtimeTags:
        return QStringLiteral("realtimeTagsPage");
    case NavigationPage::Trend:
        return QStringLiteral("trendPage");
    case NavigationPage::AlarmCenter:
        return QStringLiteral("alarmCenterPage");
    case NavigationPage::History:
        return QStringLiteral("historyPage");
    case NavigationPage::MeasurementMap:
        return QStringLiteral("measurementMapPage");
    case NavigationPage::LogsSettings:
        return QStringLiteral("logsSettingsPage");
    }

    return QString();
}

Q_DECLARE_METATYPE(NavigationPage)

#endif // NAVIGATIONPAGE_H
