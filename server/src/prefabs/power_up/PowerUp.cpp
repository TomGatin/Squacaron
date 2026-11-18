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

void buildPowerUp(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params) {
    registry.addComponent<ecs::Transform>(e, makeTransform(params));
    registry.addComponent<ecs::Renderable>(e, ecs::Renderable{.layer = 1});
    registry.addComponent<ecs::RigidBody2D>(e, ecs::RigidBody2D{
                                                   .bodyType = ecs::BodyType::Kinematic,
                                                   .velocityX = params.get("speed", DEFAULT_POWER_UP_DRIFT_SPEED),
                                               });
    registry.addComponent<ecs::Collider2D>(e, ecs::Collider2D{
                                                  .shape = ecs::ColliderShape::Circle,
                                                  .radius = POWER_UP_RADIUS,
                                                  .categoryBits = collision::POWER_UP,
                                                  .maskBits = collision::POWER_UP_MASK,
                                                  .isSensor = true,
                                              });
}

}  // namespace prefabs
