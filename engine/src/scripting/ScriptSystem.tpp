/**
 * @file ScriptSystem.tpp
 * @brief Template implementation of the ScriptSystem event forwarding.
 */

#pragma once
#include <string>
#include <utility>

#include "scripting/ScriptSystem.hpp"

namespace sq::scripting {

template <typename EventType, typename ArgExtractor>
void ScriptSystem::registerEventForwarding(sq::events::EventBus& bus, std::string const& methodName,
                                           ArgExtractor&& extractor) {
    std::string const signature = methodName + "(_)";
    if (!_cachedMethodHandles.contains(signature)) {
        _cachedMethodHandles[signature] = _vm.makeCallHandle(signature);
    }
    bus.subscribe<EventType>(
        [this, signature, extractor = std::forward<ArgExtractor>(extractor)](const EventType& event) {
            auto [targetEntity, argEntity] = extractor(event);
            invokeEntityMethod(targetEntity, signature, static_cast<double>(argEntity));
        });
}

template <typename ContactEvent>
void ScriptSystem::forwardCollisions(sq::events::EventBus& bus) {
    stopForwardingCollisions();
    prepareCollisionForwarding();
    // The system is not movable: the subscription can capture `this`; the destructor unsubscribes.
    const sq::events::EventBus::EventToken token = bus.subscribe<ContactEvent>([this](const ContactEvent& event) {
        forwardCollision(event.entityA, event.entityB);
        forwardCollision(event.entityB, event.entityA);
    });
    _stopCollisions = [&bus, token] { bus.unsubscribe<ContactEvent>(token); };
}

template <typename Extension>
Extension* ScriptSystem::findExtension() const noexcept {
    for (IScriptExtension* extension : _extensions) {
        if (auto* found = dynamic_cast<Extension*>(extension)) {
            return found;
        }
    }
    return nullptr;
}

}  // namespace sq::scripting
