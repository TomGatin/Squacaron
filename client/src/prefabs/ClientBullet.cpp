/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Client-side player bullet prefab: animated sprite driven by the physics system.
*/

#include "prefabs/ClientBullet.hpp"

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

void buildClientBullet(sq::ecs::Entity e, sq::ecs::Registry& registry, sq::prefabs::PrefabParams const& params) {
    const sq::ecs::TextureId texId = prefabTexture(registry, params);
    const float speed = params.get("speed", DEFAULT_CLIENT_BULLET_SPEED);

    registry.addComponent<sq::ecs::Transform>(e, sq::prefabs::makeTransform(params));
    registry.addComponent<sq::ecs::Renderable>(e, sq::ecs::Renderable{
                                                      .textureId = texId,
                                                      .sourceRect = {.x = 0, .y = 0, .width = 32, .height = 32},
                                                      .origin = {.x = 0.5F, .y = 0.5F},
                                                      .tint = {.r = 255, .g = 255, .b = 255, .a = 255},
                                                      .layer = 2,
                                                  });
    registry.addComponent<sq::ecs::AnimationComponent>(e, sq::ecs::AnimationComponent{
                                                              .frameWidth = 32,
                                                              .frameHeight = 32,
                                                              .frameCount = 4,
                                                              .startX = 0,
                                                              .startY = 0,
                                                              .frameDuration = 0.015F,
                                                              .loop = false,
                                                          });
    registry.addComponent<sq::ecs::RigidBody2D>(e, sq::ecs::RigidBody2D{
                                                       .bodyType = sq::ecs::BodyType::Dynamic,
                                                       .velocityX = speed,
                                                       .fixedRotation = true,
                                                   });
    registry.addComponent<sq::ecs::Collider2D>(e, sq::ecs::Collider2D{
                                                      .halfWidth = 8.0F,
                                                      .halfHeight = 3.0F,
                                                      .categoryBits = ::prefabs::collision::PLAYER_SHOT,
                                                      .maskBits = ::prefabs::collision::PLAYER_SHOT_MASK,
                                                      .isSensor = true,
                                                  });
    registry.addComponent<ClientBullet>(e, ClientBullet{});
}

}  // namespace client::prefabs
