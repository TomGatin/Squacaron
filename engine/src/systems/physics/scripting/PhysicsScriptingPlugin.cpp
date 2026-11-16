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

namespace sq::systems {

void PhysicsScriptingPlugin::build(sq::engine::PluginBuilder& builder) {
    builder.require<sq::scripting::ScriptPlugin>();
}

void PhysicsScriptingPlugin::init(sq::engine::Engine& engine) {
    _scripts = &engine.getPlugin<sq::scripting::ScriptPlugin>().scriptSystem();
    _scripts->forwardCollisions<sq::events::CollisionBeginEvent>(engine.eventBus());
}

void PhysicsScriptingPlugin::shutdown(sq::engine::Engine& /*engine*/) {
    if (_scripts != nullptr) {
        _scripts->stopForwardingCollisions();
    }
    _scripts = nullptr;
}

}  // namespace sq::systems
