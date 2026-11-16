/**
 * @file ScriptInstances.cpp
 * @brief Script instances of the entities: creation, `onSpawn()`, `onDestroy()` and deferred destruction.
 */

#include <utility>

#include "assets/AssetPath.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "log/Logger.hpp"
#include "scripting/ScriptComponent.hpp"
#include "scripting/ScriptSystem.hpp"
#include "scripting/WrenVM.hpp"
#include "scripting/detail/LogTag.hpp"
#include "scripting/detail/ScriptSupport.hpp"

namespace scripting {

using detail::LOG_TAG;

::WrenHandle* ScriptSystem::getScriptInstance(ecs::Entity e) const noexcept {
    auto it = _instanceHandles.find(e);
    return it != _instanceHandles.end() ? it->second.get() : nullptr;
}

detail::UniqueWrenHandle ScriptSystem::createScriptInstance(ecs::Entity e, const ScriptComponent& script) {
    if (!_vm.hasVariable(script.moduleName, script.className)) {
        if (_errors.reportOnce(script.moduleName + ':' + script.className)) {
            engine::log::warning(LOG_TAG, "class ", assets::quoteForLog(script.className),
                                 " not found in script module ", assets::quoteForLog(script.moduleName));
        }
        return {};
    }

    detail::UniqueWrenHandle entityArg;
    if (_vm.hasVariable("core/Entity", "Entity")) {
        entityArg = _vm.newInstanceWithHandle(_newHandle, "core/Entity", "Entity", static_cast<double>(e));
    }
    if (entityArg) {
        return _vm.newInstanceWithHandle(_newHandle, script.moduleName, script.className, entityArg.get());
    }
    return _vm.newInstanceWithHandle(_newHandle, script.moduleName, script.className, static_cast<double>(e));
}

void ScriptSystem::instantiateScript(ecs::Entity e, ScriptComponent& script, bool isReload) {
    if (script.initialized || !ensureScriptModuleLoaded(script.moduleName)) {
        return;
    }
    auto instance = createScriptInstance(e, script);
    if (!instance) {
        return;
    }
    _instanceHandles[e] = std::move(instance);
    script.initialized = true;

    if (!isReload && _vm.hasVariable("_internal", "Dispatcher")) {
        const OwnerScope scope(*this, e);
        auto trySpawnHandle = _vm.makeCallHandle("tryCallSpawn(_)");
        _vm.ensureSlots(2);
        _vm.getVariable("_internal", "Dispatcher", 0);
        _vm.setSlot(1, _instanceHandles[e].get());
        recordResult(e, _vm.call(trySpawnHandle.get()), "onSpawn()");
    }
    if (script.hasUpdate) {
        _updatingEntities.push_back(e);
    }
}

void ScriptSystem::triggerDestroyHook(ecs::Entity e) {
    auto it = _instanceHandles.find(e);
    if (it == _instanceHandles.end()) {
        return;
    }
    if (_vm.hasVariable("_internal", "Dispatcher")) {
        const OwnerScope scope(*this, e);
        auto tryDestroyHandle = _vm.makeCallHandle("tryCallDestroy(_)");
        _vm.ensureSlots(2);
        _vm.getVariable("_internal", "Dispatcher", 0);
        _vm.setSlot(1, it->second.get());
        if (_vm.call(tryDestroyHandle.get()) != WREN_RESULT_SUCCESS) {
            engine::log::warning(LOG_TAG, "entity ", e,
                                 ": onDestroy() failed: ", detail::formatScriptError(_vm.lastError()));
        }
    }
    _instanceHandles.erase(it);
    _errors.forget(e);

    if (_registry != nullptr && _registry->getComponents<ScriptComponent>().contains(e)) {
        _registry->getComponents<ScriptComponent>().get(e).initialized = false;
    }
}

void ScriptSystem::queueDestroyEntity(ecs::Entity e) {
    _pendingDestroyEntities.push_back(e);
    _commandBuffer.deferDestroy(e);
}

}  // namespace scripting
