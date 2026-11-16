/**
 * @file ScriptPlugin.cpp
 * @brief Implementation of ScriptPlugin.
 */

#include "scripting/ScriptPlugin.hpp"

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "ecs/time/Time.hpp"

namespace sq::scripting {

void ScriptPlugin::build(sq::engine::PluginBuilder& builder) { builder.require<sq::engine::CorePlugin>(); }

void ScriptPlugin::init(sq::engine::Engine& engine) { _scriptSystem.init(engine.registry(), engine.eventBus()); }

void ScriptPlugin::update(sq::engine::Engine& /*engine*/, const sq::time::Time& time) {
    if (_tick == ScriptTick::EveryFrame) {
        _scriptSystem.update(time.deltaTime);
    }
}

void ScriptPlugin::fixedUpdate(sq::engine::Engine& /*engine*/, const sq::time::Time& time) {
    if (_tick == ScriptTick::FixedStep) {
        _scriptSystem.update(time.deltaTime);
    }
}

void ScriptPlugin::shutdown(sq::engine::Engine& /*engine*/) { _scriptSystem.stopForwardingCollisions(); }

}  // namespace sq::scripting
