#include "CameraPlugin.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <variant>

#include "Arena.hpp"
#include "ArenaViewPlugin.hpp"
#include "CarViewPlugin.hpp"
#include "ClientEvents.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Tag.hpp"
#include "game/Components.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/NetworkEntityMap.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/ReplicationEvents.hpp"
#include "protocol/Protocol.hpp"
#include "render/RenderPlugin.hpp"
#include "render/api/IWindowService.hpp"
#include "render/api/components/CameraComponent.hpp"
#include "scripting/ScriptComponent.hpp"

namespace rlss::client {

namespace {

game::CameraRig* rigOf(sq::ecs::Registry& registry, const sq::ecs::Entity camera) {
    auto& rigs = registry.getComponents<game::CameraRig>();
    return rigs.contains(camera) ? &rigs.get(camera) : nullptr;
}

}  // namespace

void CameraPlugin::build(sq::engine::PluginBuilder& builder) {
    builder.require<sq::render::RenderPlugin>();
    builder.require<ArenaViewPlugin>();
    builder.require<CarViewPlugin>();
    builder.require<sq::net::NetworkPlugin>();
    builder.require<sq::net::ClientReplicationPlugin>();
}

void CameraPlugin::init(sq::engine::Engine& engine) {
    sq::ecs::Registry& registry = engine.registry();
    const auto& window = engine.service<sq::engine::IWindowService>();
    const auto width = static_cast<float>(window.width());
    const auto height = static_cast<float>(window.height());

    _camera = registry.spawnEntity();
    registry.addComponent(_camera, sq::ecs::CameraComponent{
                                       .targetX = (arena::LEFT + arena::RIGHT) / 2.0F,
                                       .targetY = (arena::TOP + arena::BOTTOM) / 2.0F,
                                       .offsetX = width / 2.0F,
                                       .offsetY = height / 2.0F,
                                   });
    registry.addComponent(_camera, sq::ecs::Tag{.name = std::string(game::CAMERA_TAG)});
    // What is drawn around the play area: the walls, and the goals on the sides.
    registry.addComponent(_camera, game::CameraRig{
                                       .viewWidth = width,
                                       .viewHeight = height,
                                       .boundsLeft = arena::LEFT - arena::GOAL_DEPTH - ARENA_WALL,
                                       .boundsTop = arena::TOP - ARENA_WALL,
                                       .boundsRight = arena::RIGHT + arena::GOAL_DEPTH + ARENA_WALL,
                                       .boundsBottom = arena::BOTTOM + ARENA_WALL,
                                   });
    registry.addComponent(_camera, sq::scripting::ScriptComponent{
                                       .moduleName = std::string(game::CAMERA_SCRIPT_MODULE),
                                       .className = "CameraDirector",
                                       .hasUpdate = true,
                                   });

    auto& events = engine.eventBus();
    _packets = events.subscribe<sq::net::PacketReceived>([this, &engine](const sq::net::PacketReceived& packet) {
        const auto message = protocol::decode(packet.data);
        if (!message) {
            return;
        }
        if (const auto* welcome = std::get_if<protocol::Welcome>(&*message)) {
            _localCar = welcome->carId;
        } else if (const auto* status = std::get_if<protocol::MatchStatus>(&*message)) {
            if (auto* rig = rigOf(engine.registry(), _camera)) {
                rig->phase = static_cast<int>(status->phase);
            }
        }
    });
    _spawned = events.subscribe<sq::net::ReplicatedEntitySpawned>(
        [this, &engine](const sq::net::ReplicatedEntitySpawned& event) {
            auto* rig = rigOf(engine.registry(), _camera);
            if (rig != nullptr && event.kind == game::KIND_BALL) {
                rig->ball = static_cast<std::int64_t>(event.entity);
            }
        });
    // A new match: its `Welcome` names the local car; the network id of the previous one means nothing anymore.
    _joined = events.subscribe<MatchJoined>([this](const MatchJoined& /*event*/) { _localCar.reset(); });
    _left = events.subscribe<MatchLeft>([this, &engine](const MatchLeft& /*event*/) {
        _localCar.reset();
        if (auto* rig = rigOf(engine.registry(), _camera)) {
            rig->phase = static_cast<int>(protocol::MatchPhase::Playing);
        }
    });
    _despawned = events.subscribe<sq::net::ReplicatedEntityDespawned>(
        [this, &engine](const sq::net::ReplicatedEntityDespawned& event) {
            auto* rig = rigOf(engine.registry(), _camera);
            if (rig != nullptr && std::cmp_equal(rig->ball, event.entity)) {
                rig->ball = game::NO_ENTITY;
            }
        });
}

void CameraPlugin::update(sq::engine::Engine& engine, const sq::time::Time& /*time*/) {
    sq::ecs::Registry& registry = engine.registry();
    auto* rig = rigOf(registry, _camera);
    if (rig == nullptr) {
        return;
    }
    rig->car = game::NO_ENTITY;
    if (_localCar && registry.hasResource<sq::net::NetworkEntityMap>()) {
        if (const auto car = registry.getResource<sq::net::NetworkEntityMap>().entityOf(*_localCar)) {
            rig->car = static_cast<std::int64_t>(*car);
        }
    }
}

void CameraPlugin::shutdown(sq::engine::Engine& engine) {
    auto& events = engine.eventBus();
    if (_packets) {
        events.unsubscribe<sq::net::PacketReceived>(*_packets);
        _packets.reset();
    }
    if (_spawned) {
        events.unsubscribe<sq::net::ReplicatedEntitySpawned>(*_spawned);
        _spawned.reset();
    }
    if (_despawned) {
        events.unsubscribe<sq::net::ReplicatedEntityDespawned>(*_despawned);
        _despawned.reset();
    }
    if (_joined) {
        events.unsubscribe<MatchJoined>(*_joined);
        _joined.reset();
    }
    if (_left) {
        events.unsubscribe<MatchLeft>(*_left);
        _left.reset();
    }
}

}  // namespace rlss::client
