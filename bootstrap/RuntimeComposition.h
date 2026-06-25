#ifndef RUNTIMECOMPOSITION_H
#define RUNTIMECOMPOSITION_H

#include "RuntimeCompositionDependencies.h"

#include "application/events/EventBus.h"

#include <QStringList>

#include <memory>

namespace Monitor::Bootstrap {

class RuntimeComposition
{
public:
    RuntimeComposition();
    explicit RuntimeComposition(RuntimeCompositionDependencies dependencies);
    ~RuntimeComposition();

    RuntimeComposition(const RuntimeComposition &) = delete;
    RuntimeComposition &operator=(const RuntimeComposition &) = delete;

    RuntimeComposition(RuntimeComposition &&) noexcept;
    RuntimeComposition &operator=(RuntimeComposition &&) noexcept;

    bool initialize(QStringList *errors = nullptr);
    bool isInitialized() const;

    const RuntimeCompositionDependencies &dependencies() const;
    Monitor::Application::EventBus *eventBus();
    const Monitor::Application::EventBus *eventBus() const;
    QStringList layerTargetNames() const;
    QStringList registeredConsumerNames() const;

private:
    RuntimeCompositionDependencies m_dependencies;
    std::unique_ptr<Monitor::Application::EventBus> m_eventBus;
    QStringList m_layerTargetNames;
    bool m_initialized = false;
};

} // namespace Monitor::Bootstrap

#endif // RUNTIMECOMPOSITION_H
