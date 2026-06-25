#include "RuntimeComposition.h"

#include "EventRegistration.h"

#include "application/ApplicationLayer.h"
#include "domain/DomainLayer.h"
#include "infrastructure/InfrastructureLayer.h"
#include "presentation/PresentationLayer.h"
#include "simulator/SimulatorLayer.h"

#include "phase0/SourceBehaviorFreeze.h"

#include <utility>

namespace Monitor::Bootstrap {
namespace {

void appendErrors(QStringList *target, const QStringList &source)
{
    if (target) {
        target->append(source);
    }
}

QStringList collectLayerTargetNames()
{
    return {
        Monitor::Domain::domainLayerInfo().name,
        Monitor::Application::applicationLayerInfo().name,
        Monitor::Infrastructure::infrastructureLayerInfo().name,
        Monitor::Simulator::simulatorLayerInfo().name,
        Monitor::Presentation::presentationLayerInfo().name
    };
}

} // namespace

RuntimeComposition::RuntimeComposition()
    : RuntimeComposition(RuntimeCompositionDependencies::createDefault())
{
}

RuntimeComposition::RuntimeComposition(RuntimeCompositionDependencies dependencies)
    : m_dependencies(std::move(dependencies))
{
}

RuntimeComposition::~RuntimeComposition() = default;
RuntimeComposition::RuntimeComposition(RuntimeComposition &&) noexcept = default;
RuntimeComposition &RuntimeComposition::operator=(RuntimeComposition &&) noexcept = default;

bool RuntimeComposition::initialize(QStringList *errors)
{
    QStringList localErrors;
    if (!errors) {
        errors = &localErrors;
    }

    errors->clear();

    QStringList phase0Errors;
    if (!Phase0::validateSourceBehaviorFreeze(&phase0Errors)) {
        appendErrors(errors, phase0Errors);
    }

    appendErrors(errors, m_dependencies.validate());

    m_layerTargetNames = collectLayerTargetNames();
    if (m_layerTargetNames.size() != 5 ||
        !m_layerTargetNames.contains(QStringLiteral("MonitorDomain")) ||
        !m_layerTargetNames.contains(QStringLiteral("MonitorApplication")) ||
        !m_layerTargetNames.contains(QStringLiteral("MonitorInfrastructure")) ||
        !m_layerTargetNames.contains(QStringLiteral("MonitorSimulator")) ||
        !m_layerTargetNames.contains(QStringLiteral("MonitorPresentation"))) {
        errors->append(QStringLiteral("Runtime composition must expose the five stage-1 CMake layer targets."));
    }

    m_eventBus = std::make_unique<Monitor::Application::EventBus>();
    EventRegistration::registerApplicationEvents(*m_eventBus, errors);

    m_initialized = errors->isEmpty();
    return m_initialized;
}

bool RuntimeComposition::isInitialized() const
{
    return m_initialized;
}

const RuntimeCompositionDependencies &RuntimeComposition::dependencies() const
{
    return m_dependencies;
}

Monitor::Application::EventBus *RuntimeComposition::eventBus()
{
    return m_eventBus.get();
}

const Monitor::Application::EventBus *RuntimeComposition::eventBus() const
{
    return m_eventBus.get();
}

QStringList RuntimeComposition::layerTargetNames() const
{
    return m_layerTargetNames;
}

QStringList RuntimeComposition::registeredConsumerNames() const
{
    QStringList consumers;
    if (!m_eventBus) {
        return consumers;
    }

    for (const auto &subscription : m_eventBus->subscriptions()) {
        if (!consumers.contains(subscription.consumerName)) {
            consumers.append(subscription.consumerName);
        }
    }

    return consumers;
}

} // namespace Monitor::Bootstrap
