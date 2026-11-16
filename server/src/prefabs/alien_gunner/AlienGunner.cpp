#include "prefabs/alien_gunner/AlienGunner.hpp"

#include <algorithm>
#include <cmath>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "prefabs/CollisionCategories.hpp"
#include "prefabs/EnemyGun.hpp"
#include "prefabs/PrefabParams.hpp"
#include "render/api/components/Renderable.hpp"
#include "systems/physics/components/Collider2D.hpp"
#include "systems/physics/components/RigidBody2D.hpp"

namespace prefabs {

namespace {

/**
 * @brief Reads a number parameter and clamps it, whatever a level file gives (huge, negative, NaN).
 * @param params Spawn parameters.
 * @param key Parameter name.
 * @param fallback Value when the key is missing or NaN.
 * @param low Smallest value.
 * @param high Largest value.
 * @return The value, within [low, high].
 */
float clampedParam(sq::prefabs::PrefabParams const& params, const char* key, const float fallback, const float low,
                   const float high) {
    const float requested = params.get(key, fallback);
    return std::isnan(requested) ? fallback : std::clamp(requested, low, high);
}

}  // namespace

void buildAlienGunner(sq::ecs::Entity e, sq::ecs::Registry& registry, sq::prefabs::PrefabParams const& params) {
    const float interval = clampedParam(params, "fire_interval", DEFAULT_ALIEN_GUNNER_FIRE_INTERVAL, 0.3F, 10.0F);
    registry.addComponent<sq::ecs::Transform>(e, sq::prefabs::makeTransform(params));
    registry.addComponent<sq::ecs::Renderable>(e, sq::ecs::Renderable{.layer = 1});
    registry.addComponent<sq::ecs::RigidBody2D>(e, sq::ecs::RigidBody2D{
                                                       .bodyType = sq::ecs::BodyType::Kinematic,
                                                       .velocityX = params.get("speed", DEFAULT_ALIEN_GUNNER_SPEED),
                                                   });
    registry.addComponent<sq::ecs::Collider2D>(e, sq::ecs::Collider2D{
                                                      .halfWidth = ALIEN_GUNNER_HALF_SIZE,
                                                      .halfHeight = ALIEN_GUNNER_HALF_SIZE,
                                                      .categoryBits = collision::ENEMY,
                                                      .maskBits = collision::ENEMY_MASK,
                                                      .isSensor = true,
                                                  });
    registry.addComponent<EnemyGun>(
        e, EnemyGun{
               .interval = interval,
               .cooldown = interval / 2.0F,
               .shotSpeed = clampedParam(params, "shot_speed", DEFAULT_ALIEN_GUNNER_SHOT_SPEED, 50.0F, 900.0F),
               .shotLook = ALIEN_GUNNER_SHOT_LOOK,
           });
}

}  // namespace prefabs
