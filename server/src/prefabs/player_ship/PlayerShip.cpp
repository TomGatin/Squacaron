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

void buildPlayerShip(sq::ecs::Entity e, sq::ecs::Registry& registry, sq::prefabs::PrefabParams const& params) {
    registry.addComponent<sq::ecs::Transform>(e, sq::prefabs::makeTransform(params));
    registry.addComponent<sq::ecs::Renderable>(e, sq::ecs::Renderable{.layer = 2});
    registry.addComponent<sq::ecs::RigidBody2D>(e, sq::ecs::RigidBody2D{.bodyType = sq::ecs::BodyType::Dynamic});
    registry.addComponent<sq::ecs::Collider2D>(e, sq::ecs::Collider2D{
                                                      .halfWidth = PLAYER_SHIP_HALF_WIDTH,
                                                      .halfHeight = PLAYER_SHIP_HALF_HEIGHT,
                                                      .categoryBits = collision::PLAYER,
                                                      .maskBits = collision::PLAYER_MASK,
                                                      .isSensor = true,
                                                  });
}

}  // namespace prefabs
