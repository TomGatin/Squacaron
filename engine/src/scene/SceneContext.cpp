#include "scene/SceneContext.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>

#include "core/Engine.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "render/api/components/Hidden.hpp"
#include "scene/SceneManager.hpp"
#include "scene/SceneTypes.hpp"

namespace sq::scene {

sq::engine::Engine& SceneContext::engine() const {
    sq::engine::Engine* host = _manager->engine();
    if (host == nullptr) {
        throw std::logic_error("SceneContext: the scene manager is not attached to an engine");
    }
    return *host;
}

sq::ecs::Registry& SceneContext::registry() const { return engine().registry(); }

sq::events::EventBus& SceneContext::bus() const { return engine().eventBus(); }

bool SceneContext::isRunning() const noexcept { return _manager->isRunning(_id); }

bool SceneContext::isVisible() const noexcept { return _manager->isVisible(_id); }

sq::ecs::Entity SceneContext::spawn() {
    sq::ecs::Registry& reg = registry();
    const sq::ecs::Entity entity = reg.spawnEntity();
    reg.addComponent(entity, SceneMember{.id = _id});
    if (_manager->isActive(_id) && !isVisible()) {
        sq::ecs::hide(reg, entity, sq::ecs::HiddenReason::Scene);
    }
    return entity;
}

void SceneContext::adopt(sq::ecs::Entity entity) {
    sq::ecs::Registry& reg = registry();
    if (!reg.isAlive(entity)) {
        return;
    }
    reg.addComponent(entity, SceneMember{.id = _id});
    if (_manager->isActive(_id) && !isVisible()) {
        sq::ecs::hide(reg, entity, sq::ecs::HiddenReason::Scene);
    } else {
        sq::ecs::show(reg, entity, sq::ecs::HiddenReason::Scene);
    }
}

void SceneContext::addSystem(SceneStage stage, SceneSystem system) {
    if (!system) {
        return;
    }
    if (_runDepth > 0) {
        _pending.push_back(PendingSystem{.stage = stage, .system = std::move(system)});
        return;
    }
    _systems.at(static_cast<std::size_t>(stage)).push_back(std::move(system));
}

void SceneContext::runSystems(SceneStage stage, sq::engine::Engine& engine, const sq::time::Time& time) {
    if (_runDepth == 0 && !_pending.empty()) {
        mergePending();
    }
    auto& systems = _systems.at(static_cast<std::size_t>(stage));
    ++_runDepth;
    try {
        for (auto& system : systems) {
            system(engine, time);
        }
    } catch (...) {
        --_runDepth;
        throw;
    }
    --_runDepth;
}

void SceneContext::release() {
    if (!_subscriptions.empty()) {
        sq::events::EventBus& eventBus = bus();
        for (const Subscription& subscription : _subscriptions) {
            subscription.unsubscribe(eventBus, subscription.token);
        }
        _subscriptions.clear();
    }
    for (auto& systems : _systems) {
        systems.clear();
    }
    _pending.clear();
}

void SceneContext::mergePending() {
    for (PendingSystem& pending : _pending) {
        _systems.at(static_cast<std::size_t>(pending.stage)).push_back(std::move(pending.system));
    }
    _pending.clear();
}

}  // namespace sq::scene
