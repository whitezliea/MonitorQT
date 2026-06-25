#include "PresentationLayer.h"

namespace Monitor::Presentation {

PresentationLayerInfo presentationLayerInfo()
{
    return {
        QStringLiteral("MonitorPresentation"),
        {
            QStringLiteral("shell"),
            QStringLiteral("navigation"),
            QStringLiteral("pages"),
            QStringLiteral("viewmodels"),
            QStringLiteral("models"),
            QStringLiteral("renderers"),
            QStringLiteral("services")
        },
        {
            QStringLiteral("Dashboard"),
            QStringLiteral("Realtime Tags"),
            QStringLiteral("Trend"),
            QStringLiteral("Alarm Center"),
            QStringLiteral("History"),
            QStringLiteral("Measurement Map"),
            QStringLiteral("Logs & Settings")
        },
        {
            QStringLiteral("QtCore"),
            QStringLiteral("QtWidgets")
        }
    };
}

} // namespace Monitor::Presentation
