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

void buildObstacle(sq::ecs::Entity e, sq::ecs::Registry& registry, sq::prefabs::PrefabParams const& params) {
    registry.addComponent<sq::ecs::Transform>(e, sq::prefabs::makeTransform(params));
    registry.addComponent<sq::ecs::Renderable>(e, sq::ecs::Renderable{});
    registry.addComponent<sq::ecs::RigidBody2D>(e, sq::ecs::RigidBody2D{.bodyType = sq::ecs::BodyType::Static});
    registry.addComponent<sq::ecs::Collider2D>(e,
                                               sq::ecs::Collider2D{
                                                   .halfWidth = params.get("halfWidth", DEFAULT_OBSTACLE_HALF_WIDTH),
                                                   .halfHeight = params.get("halfHeight", DEFAULT_OBSTACLE_HALF_HEIGHT),
                                                   .categoryBits = collision::OBSTACLE,
                                                   .maskBits = collision::OBSTACLE_MASK,
                                               });
}

}  // namespace prefabs
