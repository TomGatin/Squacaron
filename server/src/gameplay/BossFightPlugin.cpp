#include "gameplay/BossFightPlugin.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "LogTag.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/time/Time.hpp"
#include "game/ReplicationCodec.hpp"
#include "gameplay/GameplayEvents.hpp"
#include "hierarchy/Hierarchy.hpp"
#include "hierarchy/HierarchyPlugin.hpp"
#include "level/Level.hpp"
#include "level/LevelLoader.hpp"
#include "log/Logger.hpp"
#include "net/ReplicationSystems.hpp"
#include "prefabs/Health.hpp"
#include "prefabs/PrefabParams.hpp"
#include "prefabs/boss_core/BossCore.hpp"
#include "prefabs/boss_part/BossPart.hpp"
#include "scripting/ScriptComponent.hpp"
#include "systems/physics/PhysicsPlugin.hpp"
#include "systems/physics/components/RigidBody2D.hpp"

namespace rtype::server::gameplay {

void BossFightPlugin::build(sq::engine::PluginBuilder& builder) {
    builder.require<sq::systems::PhysicsPlugin>();
    builder.require<sq::hierarchy::HierarchyPlugin>();
}

void BossFightPlugin::init(sq::engine::Engine& engine) {
    _bus = &engine.eventBus();
    _playedToken = _bus->subscribe<LevelEventPlayed>([this, &engine](const LevelEventPlayed& played) {
        if (played.event.type == level::EVENT_BOSS) {
            startFight(engine.registry(), played.event.params);
        }
    });
}

void BossFightPlugin::startFight(sq::ecs::Registry& registry, const sq::prefabs::PrefabParams& params) {
    // The loader only keeps the prefabs accepted by levelPrefab(): a boss event names boss_core.
    if (params.get<std::string>("prefab", "") != ::prefabs::BOSS_CORE_PREFAB) {
        return;
    }
    _fighting = true;
    // Published first, so that the events of the same step already wait for the fight.
    _bus->publish(BossFightStarted{});
    const sq::ecs::Entity boss = registry.spawnEntity();
    ::prefabs::buildBossCore(boss, registry, params);
    // The variant goes out as the subtype: the clients pick the look of the boss from it.
    registry.addComponent(boss, game::EntityNetState{
                                    .subtype = registry.getComponents<::prefabs::BossCore>().get(boss).variant,
                                    .health = registry.getComponents<::prefabs::Health>().get(boss).points,
                                });
    if (const auto script = params.find<std::string>("script")) {
        // `scripts/Boss.wren` is the module `scripts/Boss`, read under the script root (the data root, set by the
        // room), whose class is `Boss` unless the event names another one.
        const std::filesystem::path path(*script);
        registry.addComponent(boss, sq::scripting::ScriptComponent{
                                        .moduleName = (path.parent_path() / path.stem()).generic_string(),
                                        .className = params.get<std::string>("class", path.stem().string()),
                                        .hasUpdate = true,
                                    });
    }
    sq::net::replicate(registry, boss, game::KIND_BOSS_CORE);
    spawnParts(registry, boss, params);
    sq::engine::log::info(LOG_TAG, "boss fight: the scroll stops until the boss is destroyed");
}

void BossFightPlugin::spawnParts(sq::ecs::Registry& registry, const sq::ecs::Entity boss,
                                 const sq::prefabs::PrefabParams& params) {
    // A copy: spawning the parts adds transforms, which may move the storage a reference would point into.
    const sq::ecs::Transform center = registry.getComponents<sq::ecs::Transform>().get(boss);
    const auto count = static_cast<std::size_t>(std::clamp<std::int64_t>(
        params.get<std::int64_t>("parts.count", 0), 0, static_cast<std::int64_t>(level::MAX_BOSS_PARTS)));
    for (std::size_t i = 0; i < count; ++i) {
        const std::string prefix = std::format("parts.{}.", i);
        const auto key = [&prefix](const std::string_view field) { return prefix + std::string(field); };
        const auto role = params.get<std::string>(key("role"), "");
        if (!::prefabs::bossPartRole(role).has_value()) {
            sq::engine::log::warning(LOG_TAG, "boss part ", i, ": unknown role '", role, "', skipped");
            continue;
        }
        sq::prefabs::PrefabParams part{{"role", role}};
        part.set("x", static_cast<double>(center.x) + params.get(key("x"), 0.0));
        part.set("y", static_cast<double>(center.y) + params.get(key("y"), 0.0));
        for (const std::string_view field : {"width", "height", "health", "look"}) {
            if (const auto value = params.find<double>(key(field))) {
                part.set(field, *value);
            }
        }
        const sq::ecs::Entity entity = registry.spawnEntity();
        ::prefabs::buildBossPart(entity, registry, part);
        const auto& built = registry.getComponents<::prefabs::BossPart>().get(entity);
        const std::uint8_t health = registry.hasComponent<::prefabs::Health>(entity)
                                        ? registry.getComponents<::prefabs::Health>().get(entity).points
                                        : game::EntityNetState{}.health;
        registry.addComponent(entity, game::EntityNetState{.subtype = built.look, .health = health});
        // Placed first, then attached: setParent keeps the world position and computes the offset from it.
        sq::hierarchy::setParent(registry, entity, boss);
        sq::net::replicate(registry, entity, game::KIND_BOSS_PART);
    }
}

void BossFightPlugin::fixedUpdate(sq::engine::Engine& engine, const sq::time::Time& /*time*/) {
    bool bossLeft = false;
    engine.registry().runSystem<::prefabs::BossCore, sq::ecs::Transform, sq::ecs::RigidBody2D>(
        [&bossLeft](sq::ecs::Entity /*boss*/, const ::prefabs::BossCore& core, const sq::ecs::Transform& transform,
                    sq::ecs::RigidBody2D& body) {
            bossLeft = true;
            // Flying in: stops once in place. A script may move it afterwards.
            if (body.velocityX < 0.0F && transform.x <= core.stopX) {
                body.velocityX = 0.0F;
            }
        });
    if (_fighting && !bossLeft) {
        _fighting = false;
        sq::engine::log::info(LOG_TAG, "boss destroyed");
        _bus->publish(BossFightEnded{});
    }
}

void BossFightPlugin::shutdown(sq::engine::Engine& /*engine*/) {
    if (_playedToken) {
        _bus->unsubscribe<LevelEventPlayed>(*_playedToken);
        _playedToken.reset();
    }
}

}  // namespace rtype::server::gameplay
