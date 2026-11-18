/**
 * @file PhysicsScriptingPlugin.cpp
 * @brief Implementation of the contact forwarding to the scripts.
 */

#include "systems/physics/scripting/PhysicsScriptingPlugin.hpp"

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "scripting/ScriptPlugin.hpp"
#include "scripting/ScriptSystem.hpp"
#include "systems/physics/PhysicsEvents.hpp"

namespace systems {

void PhysicsScriptingPlugin::build(engine::PluginBuilder& builder) { builder.require<scripting::ScriptPlugin>(); }

void PhysicsScriptingPlugin::init(engine::Engine& engine) {
    _scripts = &engine.getPlugin<scripting::ScriptPlugin>().scriptSystem();
    _scripts->forwardCollisions<events::CollisionBeginEvent>(engine.eventBus());
}

void PhysicsScriptingPlugin::shutdown(engine::Engine& /*engine*/) {
    if (_scripts != nullptr) {
        _scripts->stopForwardingCollisions();
    }
    _scripts = nullptr;
}

}  // namespace systems
