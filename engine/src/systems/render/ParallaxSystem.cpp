/*
** EPITECH PROJECT, 2026
** Squacaron
** File description:
** Scrolls background entities tagged with ParallaxLayer.
*/

#include "systems/render/ParallaxSystem.hpp"

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "render/api/components/CameraComponent.hpp"
#include "render/api/components/ParallaxLayer.hpp"
#include "render/api/components/SpriteInstances.hpp"

namespace {

void applyWrap(float& value, float wrapMin, float wrapMax) {
    if (wrapMin == wrapMax) {
        return;
    }
    if (value < wrapMin) {
        value = wrapMax;
    } else if (value > wrapMax) {
        value = wrapMin;
    }
}

void scroll(float& x, float& y, const sq::ecs::ParallaxLayer& layer, float dx, float dy) {
    x += dx;
    y += dy;
    applyWrap(x, layer.wrapMinX, layer.wrapMaxX);
    applyWrap(y, layer.wrapMinY, layer.wrapMaxY);
}

}  // namespace

namespace sq::systems {

void ParallaxSystem::update(sq::ecs::Registry& registry, float deltaTime) {
    float cameraX = _prevCameraX;
    float cameraY = _prevCameraY;

    registry.runSystem<sq::ecs::CameraComponent>(
        [&cameraX, &cameraY](sq::ecs::Entity /*entity*/, const sq::ecs::CameraComponent& cam) {
            if (cam.active) {
                cameraX = cam.targetX;
                cameraY = cam.targetY;
            }
        });

    const float cameraDeltaX = cameraX - _prevCameraX;
    const float cameraDeltaY = cameraY - _prevCameraY;
    _prevCameraX = cameraX;
    _prevCameraY = cameraY;

    registry.runSystem<sq::ecs::Transform, sq::ecs::ParallaxLayer>(
        [deltaTime, cameraDeltaX, cameraDeltaY](sq::ecs::Entity /*entity*/, sq::ecs::Transform& transform,
                                                const sq::ecs::ParallaxLayer& layer) {
            const float dx = (layer.scrollSpeedX * deltaTime) + (layer.cameraFactorX * cameraDeltaX);
            const float dy = (layer.scrollSpeedY * deltaTime) + (layer.cameraFactorY * cameraDeltaY);
            scroll(transform.x, transform.y, layer, dx, dy);
        });

    registry.runSystem<sq::ecs::SpriteInstances, sq::ecs::ParallaxLayer>(
        [deltaTime, cameraDeltaX, cameraDeltaY](sq::ecs::Entity /*entity*/, sq::ecs::SpriteInstances& pool,
                                                const sq::ecs::ParallaxLayer& layer) {
            const float dx = (layer.scrollSpeedX * deltaTime) + (layer.cameraFactorX * cameraDeltaX);
            const float dy = (layer.scrollSpeedY * deltaTime) + (layer.cameraFactorY * cameraDeltaY);
            for (auto& inst : pool.instances) {
                scroll(inst.x, inst.y, layer, dx, dy);
            }
        });
}

}  // namespace sq::systems
