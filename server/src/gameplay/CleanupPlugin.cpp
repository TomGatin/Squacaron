#include "gameplay/CleanupPlugin.hpp"

#include <cstdint>
#include <vector>

#include "core/Engine.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/time/Time.hpp"
#include "game/ReplicationCodec.hpp"
#include "gameplay/GameplayComponents.hpp"
#include "net/components/Replicated.hpp"

namespace rtype::server::gameplay {

namespace {

/**
 * @brief Tells whether entities of a kind fly towards the players and are gone once past the left edge: enemies, their
 * shots and the power-ups (some are spawned by scripts).
 * @param kind Replicated kind.
 * @return True for the kinds removed past the left edge.
 */
bool flowsLeft(const std::uint32_t kind) noexcept {
    return kind == game::KIND_ALIEN_SCOUT || kind == game::KIND_ALIEN_GUNNER || kind == game::KIND_ALIEN_ELITE ||
           kind == game::KIND_MISSILE || kind == game::KIND_POWER_UP;
}

}  // namespace

void CleanupPlugin::fixedUpdate(sq::engine::Engine& engine, const sq::time::Time& /*time*/) {
    sq::ecs::Registry& registry = engine.registry();
    std::vector<sq::ecs::Entity> toDestroy;
    registry.runSystem<sq::ecs::Transform, sq::ecs::Replicated>([&toDestroy](const sq::ecs::Entity entity,
                                                                             const sq::ecs::Transform& transform,
                                                                             const sq::ecs::Replicated& replicated) {
        const bool bulletGone =
            replicated.kind == game::KIND_PLAYER_BULLET && transform.x > SCENE_WIDTH + OFFSCREEN_MARGIN;
        const bool enemyGone = flowsLeft(replicated.kind) && transform.x < -OFFSCREEN_MARGIN;
        // An aimed missile may leave by any edge.
        const bool missileGone = replicated.kind == game::KIND_MISSILE &&
                                 (transform.x > SCENE_WIDTH + OFFSCREEN_MARGIN || transform.y < -OFFSCREEN_MARGIN ||
                                  transform.y > SCENE_HEIGHT + OFFSCREEN_MARGIN);
        const bool terrainGone = replicated.kind == game::KIND_OBSTACLE &&
                                 transform.x < -OFFSCREEN_MARGIN - (game::TERRAIN_BLOCK_WIDTH / 2.0F);
        if (bulletGone || enemyGone || missileGone || terrainGone) {
            toDestroy.push_back(entity);
        }
    });
    for (const sq::ecs::Entity entity : toDestroy) {
        registry.killEntity(entity);
    }
}

}  // namespace rtype::server::gameplay
