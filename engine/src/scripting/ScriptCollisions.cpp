/**
 * @file ScriptCollisions.cpp
 * @brief Forwarding of events to entity scripts, and of physics contacts to `onCollision(other)`.
 */

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

#include "ecs/Entity.hpp"
#include "scripting/ScriptComponent.hpp"
#include "scripting/ScriptSystem.hpp"
#include "scripting/detail/WrenTypes.hpp"

namespace scripting {

namespace {

/** @brief Wren signature of the collision hook; a static string so lookups do not allocate. */
const std::string ON_COLLISION_SIGNATURE = "onCollision(_)";

}  // namespace

void ScriptSystem::invokeEntityMethod(ecs::Entity target, std::string const& signature, double arg) {
    auto itInst = _instanceHandles.find(target);
    if (itInst == _instanceHandles.end()) {
        return;
    }
    auto itMethod = _cachedMethodHandles.find(signature);
    if (itMethod == _cachedMethodHandles.end() || itMethod->second == nullptr) {
        return;
    }

    const OwnerScope scope(*this, target);
    detail::UniqueWrenHandle argObj;
    if (_vm.hasVariable("core/Entity", "Entity")) {
        argObj = _vm.newInstanceWithHandle(_newHandle, "core/Entity", "Entity", arg);
    }
    CallResult const res = argObj ? _vm.call(itMethod->second, itInst->second.get(), argObj.get())
                                  : _vm.call(itMethod->second, itInst->second.get(), arg);
    recordResult(target, res, signature);
}

bool ScriptSystem::isPendingDestroy(ecs::Entity e) const noexcept {
    return std::ranges::find(_pendingDestroyEntities, e) != _pendingDestroyEntities.end();
}

void ScriptSystem::forwardCollision(ecs::Entity target, ecs::Entity other) {
    if (_registry == nullptr || isScriptDisabled(target) || !_registry->hasComponent<ScriptComponent>(target) ||
        !_registry->getComponents<ScriptComponent>().get(target).hasCollision) {
        return;
    }
    if (!_registry->isAlive(target) || !_registry->isAlive(other) || isPendingDestroy(target) ||
        isPendingDestroy(other)) {
        return;
    }
    invokeEntityMethod(target, ON_COLLISION_SIGNATURE, static_cast<double>(other));
}

void ScriptSystem::prepareCollisionForwarding() {
    if (!_cachedMethodHandles.contains(ON_COLLISION_SIGNATURE)) {
        _cachedMethodHandles[ON_COLLISION_SIGNATURE] = _vm.makeCallHandle(ON_COLLISION_SIGNATURE);
    }
}

void ScriptSystem::stopForwardingCollisions() {
    if (_stopCollisions) {
        const std::function<void()> stop = std::exchange(_stopCollisions, {});
        stop();
    }
}

}  // namespace scripting
