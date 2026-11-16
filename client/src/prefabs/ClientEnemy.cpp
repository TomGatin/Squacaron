/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Client-side Bydos enemy prefab: animated sprite flying left, hittable by player shots.
*/

#include "prefabs/ClientEnemy.hpp"

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "prefabs/CollisionCategories.hpp"
#include "prefabs/PrefabParams.hpp"
#include "prefabs/PrefabTexture.hpp"
#include "render/api/components/AnimationComponent.hpp"
#include "render/api/components/Renderable.hpp"
#include "systems/physics/components/Collider2D.hpp"
#include "systems/physics/components/RigidBody2D.hpp"

namespace client::prefabs {

namespace {

constexpr int FRAME_WIDTH = 33;
constexpr int FRAME_HEIGHT = 36;
/** @brief Collider half extents before the transform scale, a bit smaller than the frame to ignore its margins. */
constexpr float HALF_WIDTH = 14.0F;
constexpr float HALF_HEIGHT = 15.0F;

}  // namespace

void buildClientEnemy(sq::ecs::Entity e, sq::ecs::Registry& registry, sq::prefabs::PrefabParams const& params) {
    const sq::ecs::TextureId texId = prefabTexture(registry, params);
    const float speed = params.get("speed", DEFAULT_CLIENT_ENEMY_SPEED);

    registry.addComponent<sq::ecs::Transform>(e, sq::prefabs::makeTransform(params));
    registry.addComponent<sq::ecs::Renderable>(
        e, sq::ecs::Renderable{
               .textureId = texId,
               .sourceRect = {.x = 0, .y = 0, .width = FRAME_WIDTH, .height = FRAME_HEIGHT},
               .origin = {.x = 0.5F, .y = 0.5F},
               .tint = {.r = 255, .g = 255, .b = 255, .a = 255},
               .layer = 2,
           });
    registry.addComponent<sq::ecs::AnimationComponent>(e, sq::ecs::AnimationComponent{
                                                              .frameWidth = FRAME_WIDTH,
                                                              .frameHeight = FRAME_HEIGHT,
                                                              .frameCount = 8,
                                                              .startX = 0,
                                                              .startY = 0,
                                                              .frameDuration = 0.12F,
                                                          });
    registry.addComponent<sq::ecs::RigidBody2D>(e, sq::ecs::RigidBody2D{
                                                       .bodyType = sq::ecs::BodyType::Kinematic,
                                                       .velocityX = -speed,
                                                       .fixedRotation = true,
                                                   });
    registry.addComponent<sq::ecs::Collider2D>(e, sq::ecs::Collider2D{
                                                      .halfWidth = HALF_WIDTH,
                                                      .halfHeight = HALF_HEIGHT,
                                                      .categoryBits = ::prefabs::collision::ENEMY,
                                                      .maskBits = ::prefabs::collision::ENEMY_MASK,
                                                      .isSensor = true,
                                                  });
    registry.addComponent<ClientEnemy>(e, ClientEnemy{});
}

}  // namespace client::prefabs
