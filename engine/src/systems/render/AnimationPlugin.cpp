/*
** EPITECH PROJECT, 2026
** Squacaron
** File description:
** AnimationPlugin implementation.
*/

#include "systems/render/AnimationPlugin.hpp"

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/time/Time.hpp"
#include "render/api/IRenderService.hpp"
#include "systems/render/AnimationSystem.hpp"

namespace sq::systems {

void AnimationPlugin::build(sq::engine::PluginBuilder& builder) {
    builder.requireService<sq::engine::IRenderService>();
}

void AnimationPlugin::fixedUpdate(sq::engine::Engine& engine, const sq::time::Time& time) {
    AnimationSystem::update(engine.registry(), time.deltaTime);
    _parallaxSystem.update(engine.registry(), time.deltaTime);
}

}  // namespace sq::systems
