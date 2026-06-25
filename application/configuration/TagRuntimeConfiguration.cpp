#include "TagRuntimeConfiguration.h"

#include <QMutexLocker>

#include <stdexcept>

namespace Monitor::Application::Configuration {

TagRuntimeConfiguration TagRuntimeConfiguration::fromDefinition(const Monitor::Domain::Tags::TagDefinition &definition)
{
    return {
        definition.tagId,
        definition.isEnabled,
        definition.warningLow,
        definition.alarmLow,
        definition.warningHigh,
        definition.alarmHigh,
        definition.isHistorized,
        definition.historyIntervalMs.value_or(1000),
        0
    };
}

TagRuntimeConfigurationStore::TagRuntimeConfigurationStore(const QVector<TagRuntimeConfiguration> &defaults)
    : m_revision(0),
      m_snapshot(createSnapshot(defaults, m_revision))
{
}

QHash<QString, TagRuntimeConfiguration> TagRuntimeConfigurationStore::snapshot() const
{
    QMutexLocker locker(&m_mutex);
    return m_snapshot;
}

TagRuntimeConfiguration TagRuntimeConfigurationStore::get(const QString &tagId) const
{
    QMutexLocker locker(&m_mutex);
    const auto it = m_snapshot.constFind(tagId);
    if (it == m_snapshot.cend()) {
        throw std::out_of_range(QStringLiteral("Unknown TagId: %1").arg(tagId).toStdString());
    }

    return it.value();
}

void TagRuntimeConfigurationStore::replace(const QVector<TagRuntimeConfiguration> &configurations)
{
    QMutexLocker locker(&m_mutex);
    ++m_revision;
    m_snapshot = createSnapshot(configurations, m_revision);
}

QHash<QString, TagRuntimeConfiguration> TagRuntimeConfigurationStore::createSnapshot(
    const QVector<TagRuntimeConfiguration> &configurations,
    qint64 revision)
{
    QHash<QString, TagRuntimeConfiguration> snapshot;
    for (auto configuration : configurations) {
        configuration.revision = revision;
        snapshot.insert(configuration.tagId, configuration);
    }

    return snapshot;
}

} // namespace Monitor::Application::Configuration
