/**
 * @file ScriptReload.cpp
 * @brief The VM side of hot reload: release the handles, replace the VM, replay the modules, rebuild the instances.
 */

#include <string>
#include <vector>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "log/Logger.hpp"
#include "scripting/CoreModules.hpp"
#include "scripting/ScriptComponent.hpp"
#include "scripting/ScriptSystem.hpp"
#include "scripting/WrenVM.hpp"
#include "scripting/detail/LogTag.hpp"
#include "scripting/detail/ScriptSupport.hpp"

namespace sq::scripting {

using detail::LOG_TAG;

void ScriptSystem::releaseActiveHandles() {
    _entitiesToReinstantiate.clear();
    for (auto const& [e, _] : _instanceHandles) {
        _entitiesToReinstantiate.push_back(e);
        if (_registry != nullptr && _registry->getComponents<ScriptComponent>().contains(e)) {
            _registry->getComponents<ScriptComponent>().get(e).initialized = false;
        }
    }
    _instanceHandles.clear();
}

void ScriptSystem::recreateVm() {
    _fibers.clear();
    _updatingEntities.clear();
    _newHandle.reset();
    _updateHandle.reset();
    _fiberCallHandle.reset();

    std::vector<std::string> cachedSignatures;
    cachedSignatures.reserve(_cachedMethodHandles.size());
    for (auto const& [signature, _] : _cachedMethodHandles) {
        cachedSignatures.push_back(signature);
    }
    _cachedMethodHandles.clear();

    _vm.reset();
    makeCallHandles();
    for (auto const& signature : cachedSignatures) {
        _cachedMethodHandles[signature] = _vm.makeCallHandle(signature);
    }

    auto const dispatcher = _vm.interpret("_internal", INTERNAL_DISPATCHER_SOURCE);
    if (!dispatcher) {
        sq::engine::log::error(
            LOG_TAG, "hot reload: cannot run the internal dispatcher: ", detail::formatScriptError(dispatcher.error()));
    }
    prepareVm(_vm);
    _reloader.replay(_vm);
}

void ScriptSystem::reinstantiateActiveScripts() {
    if (_registry == nullptr) {
        _entitiesToReinstantiate.clear();
        return;
    }
    for (sq::ecs::Entity const e : _entitiesToReinstantiate) {
        if (!_registry->getComponents<ScriptComponent>().contains(e)) {
            continue;
        }
        instantiateScript(e, _registry->getComponents<ScriptComponent>().get(e), true);

        auto it = _instanceHandles.find(e);
        if (it != _instanceHandles.end() && _vm.hasVariable("_internal", "Dispatcher")) {
            auto tryReloadHandle = _vm.makeCallHandle("tryCallReload(_)");
            _vm.ensureSlots(2);
            _vm.getVariable("_internal", "Dispatcher", 0);
            _vm.setSlot(1, it->second.get());
            recordResult(e, _vm.call(tryReloadHandle.get()), "onReload()");
        }
    }
    _entitiesToReinstantiate.clear();
}

bool ScriptSystem::reloadModifiedScripts() {
    if (_registry == nullptr || !_reloader.hasWatches()) {
        return false;
    }
    if (_reloader.collectModified([this](WrenVM& staging) { prepareVm(staging); }).empty()) {
        return false;
    }

    notifyVmReset(VmResetPhase::Before);
    _errors.clear();  // every script gets a new chance with the new code
    releaseActiveHandles();
    recreateVm();
    reinstantiateActiveScripts();
    notifyVmReset(VmResetPhase::After);
    return true;
}

}  // namespace sq::scripting
