/**
 * @file ScriptSystem.cpp
 * @brief ScriptSystem lifecycle: init, the frame update, extensions and prefab spawns.
 */

#include "scripting/ScriptSystem.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

#include "ecs/CommandBuffer.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "log/Logger.hpp"
#include "scripting/FiberScheduler.hpp"
#include "scripting/ScriptComponent.hpp"
#include "scripting/WrenVM.hpp"
#include "scripting/detail/Bindings.hpp"
#include "scripting/detail/LogTag.hpp"
#include "utils/MoveOnlyFunction.hpp"
#include "utils/Paths.hpp"

namespace scripting {

OwnerScope::OwnerScope(ScriptSystem& system, ScriptOwner owner) noexcept
    : _system(system), _previous(system._currentOwner) {
    system._currentOwner = owner;
}

OwnerScope::~OwnerScope() { _system._currentOwner = _previous; }

ScriptSystem::ScriptSystem() = default;

ScriptSystem::~ScriptSystem() {
    detachDestroyListener();
    stopForwardingCollisions();
    releaseActiveHandles();
}

void ScriptSystem::detachDestroyListener() noexcept {
    if (_registry != nullptr && _destroyListenerId.has_value()) {
        _registry->removeDestroyListener(*_destroyListenerId);
    }
    _destroyListenerId.reset();
}

void ScriptSystem::makeCallHandles() {
    _newHandle = _vm.makeCallHandle("new(_)");
    _updateHandle = _vm.makeCallHandle("update(_)");
    _fiberCallHandle = _vm.makeCallHandle("call()");
}

void ScriptSystem::init(ecs::Registry& registry, ecs::EventBus& bus) {
    detachDestroyListener();
    _registry = &registry;
    // Entities killed by C++ code (game rules, physics, network despawn...) never go through `queueDestroyEntity`:
    // their `onDestroy()` runs here while their components still exist. The hook calls into Wren synchronously, so
    // `Registry::killEntity` must not be called from inside a foreign method; scripts use `queueDestroyEntity`.
    _destroyListenerId = registry.addDestroyListener([this](ecs::Entity e) { triggerDestroyHook(e); });
    _bus = &bus;

    _vm.setRegistry(_registry);
    _vm.setUserContext(this);
    detail::registerCoreBindings(_vm);
    installModuleLoader();
    makeCallHandles();
}

void ScriptSystem::prepareVm(WrenVM& vm) {
    if (&vm != &_vm) {
        vm.setRegistry(_registry);
        vm.setUserContext(this);
        vm.setModuleLoader(_vm.getModuleLoader());
        detail::registerCoreBindings(vm);
    }
    for (auto const& replay : _binderReplays) {
        replay(vm);
    }
    for (IScriptExtension* extension : _extensions) {
        extension->bind(vm);
    }
}

void ScriptSystem::recordResult(ScriptOwner owner, CallResult result, std::string_view hook) {
    if (result == CallResult::Success) {
        _errors.recordSuccess(owner);
    } else if (_errors.recordFailure(owner, hook, _vm.lastError())) {
        _fibers.cancel(owner);
    }
}

void ScriptSystem::update(float dt) {
    if (_registry == nullptr) {
        return;
    }
    _errors.advance(dt);
    if (_reloader.pollDue(dt)) {
        reloadModifiedScripts();
    }

    auto& scripts = _registry->getComponents<ScriptComponent>();
    for (ecs::Entity const e : scripts.getDenseEntities()) {
        auto& script = scripts.get(e);
        if (!script.initialized) {
            instantiateScript(e, script);
        }
    }

    _fibers.update(dt, [this](SleepingFiber& sleeping) {
        // A fiber of a script disabled meanwhile (e.g. by a failure of another fiber this frame) is dropped.
        if (_errors.isDisabled(sleeping.owner)) {
            return;
        }
        const OwnerScope scope(*this, sleeping.owner);
        recordResult(sleeping.owner, _vm.call(_fiberCallHandle, sleeping.fiber.get()),
                     "a fiber resumed after Engine.wait");
    });

    for (ecs::Entity const e : _updatingEntities) {
        auto it = _instanceHandles.find(e);
        if (it == _instanceHandles.end() || isScriptDisabled(e)) {
            continue;
        }
        const OwnerScope scope(*this, e);
        recordResult(e, _vm.call(_updateHandle, it->second.get(), static_cast<double>(dt)), "update(_)");
    }
    std::erase_if(_updatingEntities, [this](ecs::Entity e) { return !_instanceHandles.contains(e); });

    for (ecs::Entity const e : _pendingDestroyEntities) {
        triggerDestroyHook(e);
    }
    _pendingDestroyEntities.clear();
    _commandBuffer.flush(*_registry);
}

void ScriptSystem::addExtension(IScriptExtension& extension) {
    if (std::ranges::contains(_extensions, &extension)) {
        return;
    }
    for (ModuleSource& module : extension.modules()) {
        _extraModules[std::move(module.name)] = std::move(module.source);
    }
    _extensions.push_back(&extension);
    extension.bind(_vm);
}

void ScriptSystem::removeExtension(IScriptExtension& extension) { std::erase(_extensions, &extension); }

void ScriptSystem::notifyVmReset(VmResetPhase phase) {
    for (IScriptExtension* extension : _extensions) {
        extension->onVmReset(phase);
    }
}

bool ScriptSystem::requestPrefabSpawn(std::string_view name, double x, double y) {
    for (IScriptExtension* extension : _extensions) {
        if (extension->spawnPrefab(name, x, y)) {
            return true;
        }
    }
    engine::log::warning(detail::LOG_TAG, "Engine.spawnPrefab(", utils::quoteForLog(name),
                         "): no extension spawns prefabs (add prefabs::PrefabScriptingPlugin)");
    return false;
}

ecs::MoveOnlyFunction<void(ecs::Entity, ecs::Registry&)> ScriptSystem::spawnObserver() {
    // The system is not movable and owns the command buffer the spawns usually go through: `this` stays valid.
    return [this, owner = _currentOwner](ecs::Entity spawned, ecs::Registry& registry) {
        for (IScriptExtension* extension : _extensions) {
            extension->onScriptSpawn(registry, spawned, owner);
        }
    };
}

}  // namespace scripting
