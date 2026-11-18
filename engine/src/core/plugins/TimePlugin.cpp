/**
 * @file TimePlugin.cpp
 * @brief Implementation of TimePlugin.
 */

#include "core/plugins/TimePlugin.hpp"

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "ecs/time/Time.hpp"

namespace sq::engine {

void TimePlugin::build(PluginBuilder& builder) { builder.require<CorePlugin>(); }

void TimePlugin::init(Engine& engine) {
    if (!engine.registry().hasResource<sq::time::Time>()) {
        engine.registry().emplaceResource<sq::time::Time>(sq::time::Time{
            .deltaTime = 0.0F,
            .totalElapsedTime = 0.0F,
            .tickCount = 0,
        });
    }
}

void TimePlugin::fixedUpdate(Engine& engine, const sq::time::Time& time) {
    if (engine.registry().hasResource<sq::time::Time>()) {
        engine.registry().getResource<sq::time::Time>() = time;
    }
}

void TimePlugin::update(Engine& engine, const sq::time::Time& time) {
    if (engine.registry().hasResource<sq::time::Time>()) {
        engine.registry().getResource<sq::time::Time>() = time;
    }
}

}  // namespace sq::engine
