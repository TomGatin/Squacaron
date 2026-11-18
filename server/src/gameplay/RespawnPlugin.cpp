#include "gameplay/RespawnPlugin.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

#include "LogTag.hpp"
#include "core/Engine.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/time/Time.hpp"
#include "game/ReplicationCodec.hpp"
#include "gameplay/GameplayComponents.hpp"
#include "gameplay/GameplayEvents.hpp"
#include "log/Logger.hpp"
#include "prefabs/player_ship/PlayerShip.hpp"

namespace rtype::server::gameplay {

namespace {

/**
 * @brief Tells whether a ship overlaps a terrain block.
 * @param registry Room registry.
 * @param ship Player ship.
 * @return True if the ship is inside the terrain (touching edges do not count).
 */
bool isInTerrain(sq::ecs::Registry& registry, const sq::ecs::Entity ship) {
    if (!registry.hasComponent<sq::ecs::Transform>(ship)) {
        return false;
    }
    const auto& transform = registry.getComponents<sq::ecs::Transform>().get(ship);
    bool inside = false;
    registry.runSystem<TerrainBlock, sq::ecs::Transform>([&](sq::ecs::Entity /*block*/, const TerrainBlock& block,
                                                             const sq::ecs::Transform& blockTransform) {
        inside = inside ||
                 (std::fabs(transform.x - blockTransform.x) < ::prefabs::PLAYER_SHIP_HALF_WIDTH + block.halfWidth &&
                  std::fabs(transform.y - blockTransform.y) < ::prefabs::PLAYER_SHIP_HALF_HEIGHT + block.halfHeight);
    });
    return inside;
}

}  // namespace

void RespawnPlugin::init(sq::engine::Engine& engine) {
    _bus = &engine.eventBus();
    _destroyedToken = _bus->subscribe<ShipDestroyed>([this, &engine](const ShipDestroyed& destroyed) {
        sq::ecs::Registry& registry = engine.registry();
        if (!registry.isAlive(destroyed.ship) || registry.hasComponent<Respawning>(destroyed.ship) ||
            registry.hasComponent<Eliminated>(destroyed.ship)) {
            return;
        }
        if (registry.hasComponent<Lives>(destroyed.ship)) {
            auto& lives = registry.getComponents<Lives>().get(destroyed.ship);
            lives.left = lives.left > 0 ? static_cast<std::uint8_t>(lives.left - 1) : 0;
            if (registry.hasComponent<game::EntityNetState>(destroyed.ship)) {
                registry.getComponents<game::EntityNetState>().get(destroyed.ship).health = lives.left;
            }
            if (lives.left == 0) {
                eliminate(registry, destroyed.ship);
                return;
            }
        }
        respawn(registry, destroyed.ship);
    });
}

void giveLives(sq::ecs::Registry& registry, const sq::ecs::Entity ship, const std::uint8_t lives) {
    registry.addComponent(ship, Lives{.left = lives});
    if (registry.hasComponent<game::EntityNetState>(ship)) {
        registry.getComponents<game::EntityNetState>().get(ship).health = lives;
    }
}

void RespawnPlugin::eliminate(sq::ecs::Registry& registry, const sq::ecs::Entity ship) {
    registry.addComponent(ship, Eliminated{});
    if (registry.hasComponent<game::EntityNetState>(ship)) {
        auto& flags = registry.getComponents<game::EntityNetState>().get(ship).flags;
        flags = static_cast<std::uint8_t>((flags | game::FLAG_ELIMINATED) & ~game::FLAG_INVULNERABLE);
    }
    if (registry.hasComponent<sq::ecs::Transform>(ship)) {
        registry.getComponents<sq::ecs::Transform>().get(ship).x = ELIMINATED_SHIP_X;
    }
    sq::engine::log::info(LOG_TAG, "a ship lost its last life");
    _bus->publish(ShipEliminated{.ship = ship});
    bool anyLeft = false;
    registry.runSystem<Lives>([&registry, &anyLeft](const sq::ecs::Entity other, const Lives& /*lives*/) {
        anyLeft = anyLeft || !registry.hasComponent<Eliminated>(other);
    });
    if (!anyLeft) {
        sq::engine::log::info(LOG_TAG, "every ship is out: the game is lost");
        _bus->publish(AllShipsEliminated{});
    }
}

void RespawnPlugin::respawn(sq::ecs::Registry& registry, const sq::ecs::Entity ship) {
    std::uint8_t playerIndex = 0;
    if (registry.hasComponent<game::EntityNetState>(ship)) {
        game::EntityNetState& state = registry.getComponents<game::EntityNetState>().get(ship);
        playerIndex = state.subtype;
        state.flags |= game::FLAG_INVULNERABLE;
    }
    if (registry.hasComponent<sq::ecs::Transform>(ship)) {
        const ShipSpawnPoint spawn = shipSpawnPoint(playerIndex);
        auto& transform = registry.getComponents<sq::ecs::Transform>().get(ship);
        transform.x = spawn.x;
        transform.y = spawn.y;
    }
    registry.addComponent(ship, Respawning{.secondsLeft = RESPAWN_DURATION});
    _respawning = true;
    _bus->publish(ShipRespawnStarted{.ship = ship});
}

void RespawnPlugin::fixedUpdate(sq::engine::Engine& engine, const sq::time::Time& time) {
    sq::ecs::Registry& registry = engine.registry();
    std::vector<sq::ecs::Entity> over;
    bool stillRespawning = false;
    registry.runSystem<Respawning>([&](const sq::ecs::Entity ship, Respawning& respawning) {
        respawning.secondsLeft -= time.deltaTime;
        if (respawning.secondsLeft <= 0.0F) {
            over.push_back(ship);
        } else {
            stillRespawning = true;
        }
    });
    for (const sq::ecs::Entity ship : over) {
        registry.removeComponent<Respawning>(ship);
        if (isInTerrain(registry, ship)) {
            sq::engine::log::info(LOG_TAG, "a ship was still in the terrain at the end of its respawn");
            respawn(registry, ship);
            stillRespawning = true;
            continue;
        }
        if (registry.hasComponent<game::EntityNetState>(ship)) {
            auto& flags = registry.getComponents<game::EntityNetState>().get(ship).flags;
            flags = static_cast<std::uint8_t>(flags & ~game::FLAG_INVULNERABLE);
        }
    }
    // Recounted from the components: a ship removed while respawning (its player left) does not hold the level.
    if (_respawning && !stillRespawning) {
        _respawning = false;
        _bus->publish(AllRespawnsOver{});
    }
}

void RespawnPlugin::shutdown(sq::engine::Engine& /*engine*/) {
    if (_destroyedToken) {
        _bus->unsubscribe<ShipDestroyed>(*_destroyedToken);
        _destroyedToken.reset();
    }
}

}  // namespace rtype::server::gameplay
