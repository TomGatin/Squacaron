#include "gameplay/LevelDirectorPlugin.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "LogTag.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "events/EventBus.hpp"
#include "game/ReplicationCodec.hpp"
#include "gameplay/GameplayEvents.hpp"
#include "level/Level.hpp"
#include "log/Logger.hpp"
#include "net/ReplicationSystems.hpp"
#include "net/components/Replicated.hpp"
#include "prefabs/Health.hpp"
#include "prefabs/PrefabParams.hpp"
#include "prefabs/alien_elite/AlienElite.hpp"
#include "prefabs/alien_gunner/AlienGunner.hpp"
#include "prefabs/alien_scout/AlienScout.hpp"
#include "prefabs/boss_core/BossCore.hpp"
#include "protocol/Messages.hpp"
#include "timeline/TimelineEvent.hpp"
#include "timeline/TimelineEvents.hpp"
#include "timeline/TimelinePlugin.hpp"

namespace rtype::server::gameplay {

namespace {

/**
 * @brief Subscribes a handler and returns the token with the matching unsubscribe function.
 * @tparam Event Event type.
 * @param bus Room bus.
 * @param handler Callback.
 * @return What `shutdown()` needs to unsubscribe.
 */
template <typename Event, typename Handler>
std::pair<sq::events::EventBus::EventToken, void (*)(sq::events::EventBus&, sq::events::EventBus::EventToken)> listen(
    sq::events::EventBus& bus, Handler handler) {
    return {bus.subscribe<Event>(std::move(handler)),
            [](sq::events::EventBus& owner, const sq::events::EventBus::EventToken token) {
                owner.unsubscribe<Event>(token);
            }};
}

}  // namespace

LevelDirectorPlugin::LevelDirectorPlugin(std::shared_ptr<const level::Level> level)
    : _level(std::move(level)), _current(_level) {}

void LevelDirectorPlugin::build(sq::engine::PluginBuilder& builder) { builder.require<sq::timeline::TimelinePlugin>(); }

std::optional<std::vector<std::string>> LevelDirectorPlugin::levelPrefab(const std::string_view type,
                                                                         const std::string_view prefab) {
    if (type == level::EVENT_SPAWN && prefab == ::prefabs::ALIEN_SCOUT_PREFAB) {
        return std::vector<std::string>{"speed", "z", "rotation", "scaleX", "scaleY"};
    }
    if (type == level::EVENT_SPAWN && prefab == ::prefabs::ALIEN_GUNNER_PREFAB) {
        return std::vector<std::string>{"speed", "fire_interval", "shot_speed", "z", "rotation", "scaleX", "scaleY"};
    }
    if (type == level::EVENT_SPAWN && prefab == ::prefabs::ALIEN_ELITE_PREFAB) {
        return std::vector<std::string>{"speed",      "health", "amplitude", "frequency", "fire_interval",
                                        "shot_speed", "z",      "rotation",  "scaleX",    "scaleY"};
    }
    if (type == level::EVENT_BOSS && prefab == ::prefabs::BOSS_CORE_PREFAB) {
        return std::vector<std::string>{"speed",  "stop_x", "health",   "variant", "width",
                                        "height", "z",      "rotation", "scaleX",  "scaleY"};
    }
    return std::nullopt;
}

void LevelDirectorPlugin::init(sq::engine::Engine& engine) {
    _registry = &engine.registry();
    _bus = &engine.eventBus();
    _timeline = &engine.getPlugin<sq::timeline::TimelinePlugin>();
    sq::events::EventBus& bus = *_bus;
    _tokens.push_back(listen<sq::timeline::TimelineEventFired>(
        bus, [this](const sq::timeline::TimelineEventFired& fired) { play(fired.event); }));
    _tokens.push_back(listen<BossFightStarted>(bus, [this](const BossFightStarted& /*started*/) {
        _bossFight = true;
        holdScroll();
    }));
    _tokens.push_back(listen<BossFightEnded>(bus, [this](const BossFightEnded& /*ended*/) {
        _bossFight = false;
        releaseScroll();
    }));
    _tokens.push_back(listen<ShipRespawnStarted>(bus, [this](const ShipRespawnStarted& /*started*/) {
        _respawning = true;
        holdScroll();
    }));
    _tokens.push_back(listen<AllRespawnsOver>(bus, [this](const AllRespawnsOver& /*over*/) {
        _respawning = false;
        releaseScroll();
    }));
    startLevel();
}

void LevelDirectorPlugin::shutdown(sq::engine::Engine& /*engine*/) {
    for (const auto& [token, unsubscribe] : _tokens) {
        unsubscribe(*_bus, token);
    }
    _tokens.clear();
    _timeline = nullptr;
}

void LevelDirectorPlugin::startGame() {
    _gameStarted = true;
    announceLevel();
}

void LevelDirectorPlugin::announceLevel() {
    const std::string name = _current != nullptr ? _current->name : "";
    const std::size_t count = std::clamp<std::size_t>(level::campaignLength(_level.get()), 1, protocol::MAX_LEVELS);
    _bus->publish(protocol::LevelStart{
        .levelIndex = static_cast<std::uint8_t>(std::min(_levelIndex, count - 1)),
        .levelCount = static_cast<std::uint8_t>(count),
        .name = name.substr(0, protocol::MAX_LEVEL_NAME_LENGTH),
    });
}

void LevelDirectorPlugin::fixedUpdate(sq::engine::Engine& /*engine*/, const sq::time::Time& time) {
    if (_intermission <= 0.0F) {
        return;
    }
    _intermission -= time.deltaTime;
    if (_intermission <= 0.0F) {
        nextLevel();
    }
}

void LevelDirectorPlugin::nextLevel() {
    // The field is cleared for the new level: enemies, shots, power-ups and terrain go, the ships stay.
    std::vector<sq::ecs::Entity> leftovers;
    _registry->runSystem<sq::ecs::Replicated>(
        [&leftovers](const sq::ecs::Entity entity, const sq::ecs::Replicated& replicated) {
            if (replicated.kind != game::KIND_PLAYER_SHIP) {
                leftovers.push_back(entity);
            }
        });
    for (const sq::ecs::Entity entity : leftovers) {
        if (_registry->isAlive(entity)) {
            _registry->killEntity(entity);
        }
    }
    _current = _current->next;
    ++_levelIndex;
    _finished = false;
    startLevel();
    // A ship still respawning keeps the new level waiting, like any respawn.
    if (_respawning) {
        holdScroll();
    }
    announceLevel();
}

void LevelDirectorPlugin::startLevel() {
    if (_timeline == nullptr || _current == nullptr) {
        return;
    }
    _timeline->timeline() = _current->timeline;
    _timeline->setSpeed(_current->scrollSpeed);
    _scrollHeld = false;
    _heldEvents.clear();
    _bus->publish(LevelStarted{});
}

void LevelDirectorPlugin::play(const sq::timeline::TimelineEvent& event) {
    if (_finished) {
        return;
    }
    if (_scrollHeld && event.type != level::EVENT_BOSS) {
        _heldEvents.push_back(event);
        return;
    }
    _bus->publish(LevelEventPlayed{.event = event});
    if (event.type == level::EVENT_SPAWN) {
        spawnEnemy(event.params);
    } else if (event.type == level::EVENT_SCROLL) {
        _timeline->setSpeed(event.params.get("speed", _timeline->speed()));
    } else if (event.type == level::EVENT_END && _gameStarted) {
        const bool last = _current == nullptr || _current->next == nullptr;
        sq::engine::log::info(LOG_TAG, "level '", _current != nullptr ? _current->name : "", "' complete");
        _finished = true;
        _timeline->setSpeed(0.0F);
        if (!last) {
            _intermission = INTERMISSION_DURATION;
        }
        _bus->publish(LevelFinished{
            .levelIndex = static_cast<std::uint8_t>(std::min<std::size_t>(_levelIndex, protocol::MAX_LEVELS - 1)),
            .lastLevel = last,
        });
    } else if (event.type == level::EVENT_END) {
        sq::engine::log::info(LOG_TAG, "level '", _current != nullptr ? _current->name : "",
                              "' complete, restarting it");
        startLevel();
    }
}

void LevelDirectorPlugin::spawnEnemy(const sq::prefabs::PrefabParams& params) {
    // The loader only keeps the prefabs accepted by levelPrefab(), so the name is one of these.
    const auto prefab = params.get<std::string>("prefab", "");
    sq::ecs::Registry& registry = *_registry;
    if (prefab == ::prefabs::ALIEN_SCOUT_PREFAB) {
        const sq::ecs::Entity enemy = registry.spawnEntity();
        ::prefabs::buildAlienScout(enemy, registry, params);
        registry.addComponent(enemy, game::EntityNetState{});
        sq::net::replicate(registry, enemy, game::KIND_ALIEN_SCOUT);
    } else if (prefab == ::prefabs::ALIEN_GUNNER_PREFAB) {
        const sq::ecs::Entity enemy = registry.spawnEntity();
        ::prefabs::buildAlienGunner(enemy, registry, params);
        registry.addComponent(enemy, game::EntityNetState{});
        sq::net::replicate(registry, enemy, game::KIND_ALIEN_GUNNER);
    } else if (prefab == ::prefabs::ALIEN_ELITE_PREFAB) {
        const sq::ecs::Entity enemy = registry.spawnEntity();
        ::prefabs::buildAlienElite(enemy, registry, params);
        // Its hit points are replicated, so that the clients can show its hits.
        registry.addComponent(
            enemy, game::EntityNetState{.health = registry.getComponents<::prefabs::Health>().get(enemy).points});
        sq::net::replicate(registry, enemy, game::KIND_ALIEN_ELITE);
    }
}

void LevelDirectorPlugin::holdScroll() {
    if (_scrollHeld || _timeline == nullptr) {
        return;
    }
    _scrollHeld = true;
    _speedBeforeHold = _timeline->speed();
    _timeline->setSpeed(0.0F);
}

void LevelDirectorPlugin::releaseScroll() {
    if (!_scrollHeld || _bossFight || _respawning) {
        return;
    }
    _scrollHeld = false;
    _timeline->setSpeed(_speedBeforeHold);
    sq::engine::log::info(LOG_TAG, "the level goes on");
    // Played in order; a new boss among them holds the scroll again and keeps the next ones waiting.
    const std::vector<sq::timeline::TimelineEvent> waiting = std::move(_heldEvents);
    _heldEvents.clear();
    for (const sq::timeline::TimelineEvent& event : waiting) {
        play(event);
    }
}

}  // namespace rtype::server::gameplay
