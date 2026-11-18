#include "prefabs/boss_part/BossPart.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "prefabs/CollisionCategories.hpp"
#include "prefabs/Health.hpp"
#include "prefabs/PrefabParams.hpp"
#include "render/api/components/Renderable.hpp"
#include "systems/physics/components/Collider2D.hpp"
#include "systems/physics/components/RigidBody2D.hpp"

namespace prefabs {

namespace {

/** @brief Smallest part hitbox side, in pixels. */
constexpr double MIN_PART_SIZE = 8.0;
/** @brief Largest part hitbox side, in pixels. */
constexpr double MAX_PART_SIZE = 1024.0;
/** @brief Largest value of a byte parameter. */
constexpr auto BYTE_MAX = static_cast<double>(std::numeric_limits<std::uint8_t>::max());

/**
 * @brief Reads a number parameter and clamps it, whatever a level file gives (huge, negative, NaN).
 * @param params Spawn parameters.
 * @param key Parameter name.
 * @param fallback Value when the key is missing or NaN.
 * @param low Smallest value.
 * @param high Largest value.
 * @return The value, within [low, high].
 */
double clampedParam(sq::prefabs::PrefabParams const& params, const char* key, const double fallback, const double low,
                    const double high) {
    const double requested = params.get(key, fallback);
    return std::isnan(requested) ? fallback : std::clamp(requested, low, high);
}

}  // namespace

std::optional<BossPartRole> bossPartRole(const std::string_view name) {
    if (name == BOSS_PART_WEAK_POINT) {
        return BossPartRole::WeakPoint;
    }
    if (name == BOSS_PART_TURRET) {
        return BossPartRole::Turret;
    }
    if (name == BOSS_PART_ARMOR) {
        return BossPartRole::Armor;
    }
    return std::nullopt;
}

void buildBossPart(sq::ecs::Entity e, sq::ecs::Registry& registry, sq::prefabs::PrefabParams const& params) {
    const BossPartRole role = bossPartRole(params.get<std::string>("role", "")).value_or(BossPartRole::Armor);
    const auto width =
        static_cast<float>(clampedParam(params, "width", DEFAULT_BOSS_PART_SIZE, MIN_PART_SIZE, MAX_PART_SIZE));
    const auto height =
        static_cast<float>(clampedParam(params, "height", DEFAULT_BOSS_PART_SIZE, MIN_PART_SIZE, MAX_PART_SIZE));
    const auto look = static_cast<std::uint8_t>(clampedParam(params, "look", 0.0, 0.0, BYTE_MAX));

    registry.addComponent<sq::ecs::Transform>(e, sq::prefabs::makeTransform(params));
    registry.addComponent<sq::ecs::Renderable>(e, sq::ecs::Renderable{.layer = 2});
    // Kinematic without velocity: the hierarchy moves it with its boss.
    registry.addComponent<sq::ecs::RigidBody2D>(e, sq::ecs::RigidBody2D{.bodyType = sq::ecs::BodyType::Kinematic});
    registry.addComponent<sq::ecs::Collider2D>(e, sq::ecs::Collider2D{
                                                      .halfWidth = width / 2.0F,
                                                      .halfHeight = height / 2.0F,
                                                      .categoryBits = collision::ENEMY,
                                                      .maskBits = collision::ENEMY_MASK,
                                                      .isSensor = true,
                                                  });
    registry.addComponent<BossPart>(e, BossPart{.role = role, .look = look});
    if (role == BossPartRole::Turret) {
        const auto health = static_cast<std::uint8_t>(
            clampedParam(params, "health", static_cast<double>(DEFAULT_TURRET_HEALTH), 1.0, BYTE_MAX));
        registry.addComponent<Health>(e, Health{.points = health, .max = health});
    }
}

}  // namespace prefabs
