#include "prefabs/missile/Missile.hpp"

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "game/ReplicationCodec.hpp"
#include "prefabs/CollisionCategories.hpp"
#include "prefabs/PrefabParams.hpp"
#include "render/api/components/Renderable.hpp"
#include "systems/physics/components/Collider2D.hpp"
#include "systems/physics/components/RigidBody2D.hpp"

namespace prefabs {

void buildMissile(sq::ecs::Entity e, sq::ecs::Registry& registry, sq::prefabs::PrefabParams const& params) {
    registry.addComponent<sq::ecs::Transform>(e, sq::prefabs::makeTransform(params));
    registry.addComponent<sq::ecs::Renderable>(e, sq::ecs::Renderable{.layer = 1});
    registry.addComponent<sq::ecs::RigidBody2D>(e, sq::ecs::RigidBody2D{
                                                       .bodyType = sq::ecs::BodyType::Dynamic,
                                                       .velocityX = params.get("speed", DEFAULT_MISSILE_SPEED),
                                                       .velocityY = params.get("speed_y", 0.0F),
                                                       .bullet = true,
                                                   });
    registry.addComponent<sq::ecs::Collider2D>(e, sq::ecs::Collider2D{
                                                      .halfWidth = rtype::game::MISSILE_WIDTH / 2.0F,
                                                      .halfHeight = rtype::game::MISSILE_HEIGHT / 2.0F,
                                                      .categoryBits = collision::ENEMY_SHOT,
                                                      .maskBits = collision::ENEMY_SHOT_MASK,
                                                      .isSensor = true,
                                                  });
}

}  // namespace prefabs
