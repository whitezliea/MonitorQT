#include "ApplicationLayer.h"

#include "configuration/ConfigurationValidation.h"
#include "configuration/RuntimeOptionsStore.h"
#include "configuration/TagRuntimeConfiguration.h"
#include "configuration/TrendDiagnosisOptions.h"
#include "services/TagDefinitionCatalog.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <stdexcept>

namespace Monitor::Application {
namespace {

void addError(QStringList *errors, const QString &message)
{
    errors->append(message);
}

const Monitor::Domain::Tags::TagDefinition *findDefinition(
    const QVector<Monitor::Domain::Tags::TagDefinition> &definitions,
    const QString &tagId)
{
    for (const auto &definition : definitions) {
        if (definition.tagId == tagId) {
            return &definition;
        }
    }

    return nullptr;
}

bool containsChannelMapping(
    const QVector<Monitor::Domain::Tags::TagSourceMapping> &mappings,
    const QString &sourceCode,
    const QString &tagId)
{
    using Monitor::Domain::Tags::SourceType;

    return std::any_of(
        mappings.cbegin(),
        mappings.cend(),
        [&sourceCode, &tagId](const Monitor::Domain::Tags::TagSourceMapping &mapping) {
            return mapping.sourceType == SourceType::Channel
                && mapping.sourceCode.has_value()
                && mapping.sourceCode.value() == sourceCode
                && mapping.tagId == tagId;
        });
}

} // namespace

ApplicationLayerInfo applicationLayerInfo()
{
    return {
        QStringLiteral("MonitorApplication"),
        {
            QStringLiteral("configuration"),
            QStringLiteral("dto"),
            QStringLiteral("events"),
            QStringLiteral("mapping"),
            QStringLiteral("pipelines"),
            QStringLiteral("queues"),
            QStringLiteral("services"),
            QStringLiteral("usecases"),
            QStringLiteral("workers")
        },
        {
            QStringLiteral("MonitorDomain"),
            QStringLiteral("QtCore")
        },
        {
            QStringLiteral("QtWidgets"),
            QStringLiteral("QtSql concrete repositories"),
            QStringLiteral("Simulator internals")
        }
    };
}

QStringList validateApplicationLayer()
{
    QStringList errors;

    try {
        const auto options = Configuration::MonitorRuntimeOptions();
        Configuration::ConfigurationValidation::validateRuntimeOptions(options);
        if (options.dataGenerateIntervalMs != 500 ||
            options.dataSourceTimeoutPeriods != 3 ||
            options.dataSourceTimeoutMs() != 1500 ||
            options.uiRefreshIntervalMs != 1000 ||
            options.historyBatchIntervalMs != 5000 ||
            options.alarmBatchIntervalMs != 5000 ||
            options.operationLogBatchIntervalMs != 5000 ||
            options.historyRetentionDays != 30 ||
            options.trendPointCount(1) != 120 ||
            options.trendPointCount(5) != 600 ||
            options.trendPointCount(30) != 3600 ||
            options.trendBufferCapacity() != 3600) {
            addError(&errors, QStringLiteral("MonitorRuntimeOptions defaults are not aligned with the WPF source."));
        }

        Configuration::RuntimeOptionsStore runtimeStore(options);
        auto changedOptions = runtimeStore.snapshot();
        changedOptions.uiRefreshIntervalMs = 250;
        runtimeStore.replace(changedOptions);
        if (runtimeStore.snapshot().uiRefreshIntervalMs != 250) {
            addError(&errors, QStringLiteral("RuntimeOptionsStore did not replace its snapshot."));
        }

        const auto definitions = Services::TagDefinitionCatalog::createDefaults();
        if (definitions.size() != 22) {
            addError(&errors, QStringLiteral("TagDefinitionCatalog must provide exactly 22 default definitions."));
        }

        QSet<QString> tagIds;
        for (const auto &definition : definitions) {
            if (tagIds.contains(definition.tagId)) {
                addError(&errors, QStringLiteral("Duplicate TagDefinition: %1").arg(definition.tagId));
            }
            tagIds.insert(definition.tagId);
        }

        const QStringList requiredTags = {
            QStringLiteral("DEVICE.STATUS"),
            QStringLiteral("DEVICE.ONLINE"),
            QStringLiteral("DEVICE.ERROR_CODE"),
            QStringLiteral("DEVICE.QUALITY"),
            QStringLiteral("DEVICE.SEQUENCE_NO"),
            QStringLiteral("DEVICE.FRAME_INTERVAL_MS"),
            QStringLiteral("DEVICE.FRAME_LOSS_COUNT"),
            QStringLiteral("MEAS.TEMP.CH01"),
            QStringLiteral("MEAS.PRESSURE.CH01"),
            QStringLiteral("MEAS.LIGHT.CH01"),
            QStringLiteral("MEAS.VOLTAGE.CH01"),
            QStringLiteral("MEAS.CURRENT.CH01"),
            QStringLiteral("MEAS.VIBRATION.CH01"),
            QStringLiteral("MEAS.POWER.CH01"),
            QStringLiteral("MEAS.LOAD_RATIO.CH01"),
            QStringLiteral("MATRIX.LIGHT.AVG"),
            QStringLiteral("MATRIX.LIGHT.MAX"),
            QStringLiteral("MATRIX.LIGHT.MIN"),
            QStringLiteral("MATRIX.LIGHT.UNIFORMITY"),
            QStringLiteral("MATRIX.LIGHT.ABNORMAL_COUNT"),
            QStringLiteral("MATRIX.LIGHT.HOTSPOT_ROW"),
            QStringLiteral("MATRIX.LIGHT.HOTSPOT_COL")
        };

        for (const auto &tagId : requiredTags) {
            if (!tagIds.contains(tagId)) {
                addError(&errors, QStringLiteral("Required default TagDefinition is missing: %1").arg(tagId));
            }
        }

        const auto mappings = Services::TagDefinitionCatalog::createSourceMappings();
        if (mappings.size() != 22) {
            addError(&errors, QStringLiteral("TagDefinitionCatalog must provide exactly 22 source mappings."));
        }

        const QHash<QString, QString> requiredChannelMappings = {
            {QStringLiteral("TEMP_CH01"), QStringLiteral("MEAS.TEMP.CH01")},
            {QStringLiteral("PRESSURE_CH01"), QStringLiteral("MEAS.PRESSURE.CH01")},
            {QStringLiteral("LIGHT_CH01"), QStringLiteral("MEAS.LIGHT.CH01")},
            {QStringLiteral("VOLTAGE_CH01"), QStringLiteral("MEAS.VOLTAGE.CH01")},
            {QStringLiteral("CURRENT_CH01"), QStringLiteral("MEAS.CURRENT.CH01")},
            {QStringLiteral("VIBRATION_CH01"), QStringLiteral("MEAS.VIBRATION.CH01")}
        };

        for (auto it = requiredChannelMappings.cbegin(); it != requiredChannelMappings.cend(); ++it) {
            if (!containsChannelMapping(mappings, it.key(), it.value())) {
                addError(&errors, QStringLiteral("Required channel mapping is missing: %1 -> %2").arg(it.key(), it.value()));
            }
        }

        QVector<Configuration::TagRuntimeConfiguration> configurations;
        configurations.reserve(definitions.size());
        for (const auto &definition : definitions) {
            auto configuration = Configuration::TagRuntimeConfiguration::fromDefinition(definition);
            configurations.append(configuration);
        }

        Configuration::TagRuntimeConfigurationStore configurationStore(configurations);
        if (configurationStore.snapshot().size() != 22 ||
            configurationStore.get(QStringLiteral("MEAS.TEMP.CH01")).revision != 0) {
            addError(&errors, QStringLiteral("TagRuntimeConfigurationStore default snapshot is not aligned."));
        }

        const auto *tempDefinition = findDefinition(definitions, QStringLiteral("MEAS.TEMP.CH01"));
        if (!tempDefinition) {
            addError(&errors, QStringLiteral("MEAS.TEMP.CH01 definition is required for validation."));
        } else {
            Configuration::ConfigurationValidation::validateTag(
                *tempDefinition,
                Configuration::TagRuntimeConfiguration::fromDefinition(*tempDefinition));
        }

        auto replacement = configurationStore.get(QStringLiteral("MEAS.TEMP.CH01"));
        replacement.warningHigh = 55.0;
        configurations[0] = configurationStore.get(definitions.first().tagId);
        for (auto &configuration : configurations) {
            if (configuration.tagId == replacement.tagId) {
                configuration = replacement;
                break;
            }
        }
        configurationStore.replace(configurations);
        const auto replaced = configurationStore.get(QStringLiteral("MEAS.TEMP.CH01"));
        if (replaced.revision != 1 || !replaced.warningHigh.has_value() || replaced.warningHigh.value() != 55.0) {
            addError(&errors, QStringLiteral("TagRuntimeConfigurationStore replacement revision behavior is not aligned."));
        }

        if (!tempDefinition) {
            addError(&errors, QStringLiteral("MEAS.TEMP.CH01 definition is required for validation."));
        } else {
            auto invalid = Configuration::TagRuntimeConfiguration::fromDefinition(*tempDefinition);
            invalid.alarmLow = 10.0;
            invalid.warningLow = 5.0;
            auto rejectedInvalidThresholdOrder = false;
            try {
                Configuration::ConfigurationValidation::validateTag(*tempDefinition, invalid);
            } catch (const std::exception &) {
                rejectedInvalidThresholdOrder = true;
            }

            if (!rejectedInvalidThresholdOrder) {
                addError(&errors, QStringLiteral("ConfigurationValidation must reject AlarmLow > WarningLow."));
            }
        }

        Configuration::TrendDiagnosisOptions trendDiagnosisOptions;
        trendDiagnosisOptions.validate();
    } catch (const std::exception &exception) {
        addError(&errors, QStringLiteral("Application validation threw unexpectedly: %1").arg(QString::fromUtf8(exception.what())));
    }

    return errors;
}

} // namespace Monitor::Application
