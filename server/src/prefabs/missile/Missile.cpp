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

void buildMissile(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params) {
    registry.addComponent<ecs::Transform>(e, makeTransform(params));
    registry.addComponent<ecs::Renderable>(e, ecs::Renderable{.layer = 1});
    registry.addComponent<ecs::RigidBody2D>(e, ecs::RigidBody2D{
                                                   .bodyType = ecs::BodyType::Dynamic,
                                                   .velocityX = params.get("speed", DEFAULT_MISSILE_SPEED),
                                                   .velocityY = params.get("speed_y", 0.0F),
                                                   .bullet = true,
                                               });
    registry.addComponent<ecs::Collider2D>(e, ecs::Collider2D{
                                                  .halfWidth = rtype::game::MISSILE_WIDTH / 2.0F,
                                                  .halfHeight = rtype::game::MISSILE_HEIGHT / 2.0F,
                                                  .categoryBits = collision::ENEMY_SHOT,
                                                  .maskBits = collision::ENEMY_SHOT_MASK,
                                                  .isSensor = true,
                                              });
}

}  // namespace prefabs
