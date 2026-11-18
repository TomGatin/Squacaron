#include "prefabs/player_ship/PlayerShip.hpp"

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "prefabs/CollisionCategories.hpp"
#include "prefabs/PrefabParams.hpp"
#include "render/api/components/Renderable.hpp"
#include "systems/physics/components/Collider2D.hpp"
#include "systems/physics/components/RigidBody2D.hpp"

namespace prefabs {

void buildPlayerShip(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params) {
    registry.addComponent<ecs::Transform>(e, makeTransform(params));
    registry.addComponent<ecs::Renderable>(e, ecs::Renderable{.layer = 2});
    registry.addComponent<ecs::RigidBody2D>(e, ecs::RigidBody2D{.bodyType = ecs::BodyType::Dynamic});
    registry.addComponent<ecs::Collider2D>(e, ecs::Collider2D{
                                                  .halfWidth = PLAYER_SHIP_HALF_WIDTH,
                                                  .halfHeight = PLAYER_SHIP_HALF_HEIGHT,
                                                  .categoryBits = collision::PLAYER,
                                                  .maskBits = collision::PLAYER_MASK,
                                                  .isSensor = true,
                                              });
}

}  // namespace prefabs
