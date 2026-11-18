#include "prefabs/obstacle/Obstacle.hpp"

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "prefabs/CollisionCategories.hpp"
#include "prefabs/PrefabParams.hpp"
#include "render/api/components/Renderable.hpp"
#include "systems/physics/components/Collider2D.hpp"
#include "systems/physics/components/RigidBody2D.hpp"

namespace prefabs {

void buildObstacle(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params) {
    registry.addComponent<ecs::Transform>(e, makeTransform(params));
    registry.addComponent<ecs::Renderable>(e, ecs::Renderable{});
    registry.addComponent<ecs::RigidBody2D>(e, ecs::RigidBody2D{.bodyType = ecs::BodyType::Static});
    registry.addComponent<ecs::Collider2D>(e, ecs::Collider2D{
                                                  .halfWidth = params.get("halfWidth", DEFAULT_OBSTACLE_HALF_WIDTH),
                                                  .halfHeight = params.get("halfHeight", DEFAULT_OBSTACLE_HALF_HEIGHT),
                                                  .categoryBits = collision::OBSTACLE,
                                                  .maskBits = collision::OBSTACLE_MASK,
                                              });
}

}  // namespace prefabs
