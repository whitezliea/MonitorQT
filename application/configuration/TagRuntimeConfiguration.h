#ifndef TAGRUNTIMECONFIGURATION_H
#define TAGRUNTIMECONFIGURATION_H

#include "domain/tags/TagModels.h"

#include <QHash>
#include <QMutex>
#include <QString>
#include <QVector>

#include <optional>

namespace Monitor::Application::Configuration {

struct TagRuntimeConfiguration
{
    QString tagId;
    bool alarmEnabled = true;
    std::optional<double> warningLow;
    std::optional<double> alarmLow;
    std::optional<double> warningHigh;
    std::optional<double> alarmHigh;
    bool isHistorized = true;
    int historyIntervalMs = 1000;
    qint64 revision = 0;

    static TagRuntimeConfiguration fromDefinition(const Monitor::Domain::Tags::TagDefinition &definition);
};

class TagRuntimeConfigurationStore
{
public:
    explicit TagRuntimeConfigurationStore(const QVector<TagRuntimeConfiguration> &defaults = {});

    QHash<QString, TagRuntimeConfiguration> snapshot() const;
    TagRuntimeConfiguration get(const QString &tagId) const;
    void replace(const QVector<TagRuntimeConfiguration> &configurations);

private:
    static QHash<QString, TagRuntimeConfiguration> createSnapshot(
        const QVector<TagRuntimeConfiguration> &configurations,
        qint64 revision);

    mutable QMutex m_mutex;
    QHash<QString, TagRuntimeConfiguration> m_snapshot;
    qint64 m_revision = 0;
};

} // namespace Monitor::Application::Configuration

#endif // TAGRUNTIMECONFIGURATION_H
