#include "prefabs/power_up/PowerUp.hpp"

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "prefabs/CollisionCategories.hpp"
#include "prefabs/PrefabParams.hpp"
#include "render/api/components/Renderable.hpp"
#include "systems/physics/components/Collider2D.hpp"
#include "systems/physics/components/RigidBody2D.hpp"

namespace prefabs {

void buildPowerUp(sq::ecs::Entity e, sq::ecs::Registry& registry, sq::prefabs::PrefabParams const& params) {
    registry.addComponent<sq::ecs::Transform>(e, sq::prefabs::makeTransform(params));
    registry.addComponent<sq::ecs::Renderable>(e, sq::ecs::Renderable{.layer = 1});
    registry.addComponent<sq::ecs::RigidBody2D>(e, sq::ecs::RigidBody2D{
                                                       .bodyType = sq::ecs::BodyType::Kinematic,
                                                       .velocityX = params.get("speed", DEFAULT_POWER_UP_DRIFT_SPEED),
                                                   });
    registry.addComponent<sq::ecs::Collider2D>(e, sq::ecs::Collider2D{
                                                      .shape = sq::ecs::ColliderShape::Circle,
                                                      .radius = POWER_UP_RADIUS,
                                                      .categoryBits = collision::POWER_UP,
                                                      .maskBits = collision::POWER_UP_MASK,
                                                      .isSensor = true,
                                                  });
}

}  // namespace prefabs
