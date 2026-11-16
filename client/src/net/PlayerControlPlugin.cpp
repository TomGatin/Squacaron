#include "net/PlayerControlPlugin.hpp"

#include <algorithm>
#include <utility>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/time/Time.hpp"
#include "game/ReplicationCodec.hpp"
#include "game/ShipMovement.hpp"
#include "input/InputState.hpp"
#include "input/RTypeBindings.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/LocalInput.hpp"
#include "net/RTypeNetworkPlugin.hpp"
#include "net/ReplicationEvents.hpp"
#include "net/SessionEvents.hpp"
#include "net/components/InputBuffer.hpp"
#include "net/components/Interpolated.hpp"
#include "net/components/Predicted.hpp"
#include "protocol/Messages.hpp"

namespace client {

namespace protocol = rtype::protocol;

void PlayerControlPlugin::build(sq::engine::PluginBuilder& builder) {
    builder.require<sq::net::ClientReplicationPlugin>();
    builder.require<RTypeNetworkPlugin>();
}

void PlayerControlPlugin::init(sq::engine::Engine& engine) {
    auto& events = engine.eventBus();
    _started = events.subscribe<GameStarted>([this](const GameStarted& event) {
        _playerIndex = event.playerIndex;
        _paused = false;
        _autoFireLatch.reset();
    });
    const auto stop = [this](const auto& /*event*/) { _playerIndex.reset(); };
    const auto ended = events.subscribe<GameEnded>(stop);
    const auto left = events.subscribe<RoomLeft>(stop);
    const auto closed = events.subscribe<SessionClosed>(stop);
    _endings = {
        [&events, ended] { events.unsubscribe<GameEnded>(ended); },
        [&events, left] { events.unsubscribe<RoomLeft>(left); },
        [&events, closed] { events.unsubscribe<SessionClosed>(closed); },
    };
    // UDP only opens at GameStart, so a snapshot never arrives before the index of this player's ship is known.
    _spawned = events.subscribe<sq::net::ReplicatedEntitySpawned>(
        [this, &engine](const sq::net::ReplicatedEntitySpawned& event) { onEntitySpawned(engine, event); });
    // Same movement as the server: the prediction and the replay after a snapshot compute what the server computes.
    _inputs = events.subscribe<sq::net::InputApplied>([&engine](const sq::net::InputApplied& event) {
        sq::ecs::Registry& registry = engine.registry();
        if (registry.hasComponent<sq::ecs::Transform>(event.entity)) {
            rtype::game::applyShipInput(registry.getComponents<sq::ecs::Transform>().get(event.entity), event.input,
                                        event.deltaTime);
        }
    });
}

void PlayerControlPlugin::onEntitySpawned(sq::engine::Engine& engine,
                                          const sq::net::ReplicatedEntitySpawned& event) const {
    sq::ecs::Registry& registry = engine.registry();
    const bool isLocalShip =
        _playerIndex && event.kind == rtype::game::KIND_PLAYER_SHIP &&
        registry.getComponents<rtype::game::EntityNetState>().get(event.entity).subtype == *_playerIndex;
    if (isLocalShip) {
        registry.addComponent(event.entity, sq::ecs::Predicted{});
    } else {
        registry.addComponent(event.entity, sq::ecs::Interpolated{});
    }
}

void PlayerControlPlugin::fixedUpdate(sq::engine::Engine& engine, const sq::time::Time& /*time*/) {
    if (!_playerIndex || _paused) {
        return;
    }
    sq::ecs::NetInput input{.sequence = ++_sequence, .held = {}, .axes = {}};
    bool shootHeld = false;
    withLocalInput(engine.registry(), [this, &input, &shootHeld](const sq::input::InputState& state) {
        for (const auto& [action, data] : state.actions) {
            if (action == input::ACTION_READY) {
                continue;
            }
            if (_autoFire && action == rtype::game::ACTION_SHOOT) {
                shootHeld = data.held;
                continue;
            }
            if (data.held && input.held.size() < protocol::MAX_INPUT_BUTTONS) {
                input.held.push_back(action);
            }
            if (data.axis != 0.0F && input.axes.size() < protocol::MAX_INPUT_AXES) {
                input.axes.push_back({.action = action, .value = std::clamp(data.axis, -1.0F, 1.0F)});
            }
        }
    });
    // The server fires while `shoot` is held, so auto-fire only has to keep it in the held list.
    if (_autoFire && _autoFireLatch.update(shootHeld) && input.held.size() < protocol::MAX_INPUT_BUTTONS) {
        input.held.push_back(rtype::game::ACTION_SHOOT);
    }
    engine.eventBus().publish(sq::net::LocalInput{.input = std::move(input)});
}

void PlayerControlPlugin::shutdown(sq::engine::Engine& engine) {
    auto& events = engine.eventBus();
    if (_started) {
        events.unsubscribe<GameStarted>(*_started);
        _started.reset();
    }
    if (_spawned) {
        events.unsubscribe<sq::net::ReplicatedEntitySpawned>(*_spawned);
        _spawned.reset();
    }
    if (_inputs) {
        events.unsubscribe<sq::net::InputApplied>(*_inputs);
        _inputs.reset();
    }
    for (const auto& unsubscribe : _endings) {
        unsubscribe();
    }
    _endings.clear();
}

}  // namespace client
