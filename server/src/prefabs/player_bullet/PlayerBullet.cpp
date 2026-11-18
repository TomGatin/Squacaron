#include "prefabs/player_bullet/PlayerBullet.hpp"

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "prefabs/CollisionCategories.hpp"
#include "prefabs/PrefabParams.hpp"
#include "render/api/components/Renderable.hpp"
#include "systems/physics/components/Collider2D.hpp"
#include "systems/physics/components/RigidBody2D.hpp"

namespace prefabs {

void buildPlayerBullet(sq::ecs::Entity e, sq::ecs::Registry& registry, sq::prefabs::PrefabParams const& params) {
    registry.addComponent<sq::ecs::Transform>(e, sq::prefabs::makeTransform(params));
    registry.addComponent<sq::ecs::Renderable>(e, sq::ecs::Renderable{.layer = 1});
    registry.addComponent<sq::ecs::RigidBody2D>(e, sq::ecs::RigidBody2D{
                                                       .bodyType = sq::ecs::BodyType::Dynamic,
                                                       .velocityX = params.get("speed", DEFAULT_PLAYER_BULLET_SPEED),
                                                       .bullet = true,
                                                   });
    registry.addComponent<sq::ecs::Collider2D>(e, sq::ecs::Collider2D{
                                                      .halfWidth = 6.0F,
                                                      .halfHeight = 2.0F,
                                                      .categoryBits = collision::PLAYER_SHOT,
                                                      .maskBits = collision::PLAYER_SHOT_MASK,
                                                      .isSensor = true,
                                                  });
}

}  // namespace prefabs
