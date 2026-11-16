#include "CarSimulationPlugin.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

#include "Arena.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Tag.hpp"
#include "ecs/components/Transform.hpp"
#include "game/Components.hpp"
#include "game/ScriptBindings.hpp"
#include "input/ActionId.hpp"
#include "input/InputState.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkService.hpp"
#include "net/NetworkEvents.hpp"
#include "net/ReplicationEvents.hpp"
#include "net/ReplicationSystems.hpp"
#include "net/ServerReplicationPlugin.hpp"
#include "net/components/InputBuffer.hpp"
#include "protocol/Protocol.hpp"
#include "scripting/ScriptComponent.hpp"
#include "scripting/ScriptPlugin.hpp"

namespace rlss::server {

void CarSimulationPlugin::build(sq::engine::PluginBuilder& builder) {
    builder.require<sq::scripting::ScriptPlugin>();
    builder.require<sq::net::ServerReplicationPlugin>();
}

void CarSimulationPlugin::init(sq::engine::Engine& engine) {
    if (!game::installScriptBindings(engine.getPlugin<sq::scripting::ScriptPlugin>().scriptSystem())) {
        throw std::runtime_error("CarSimulationPlugin: Wren refused the RLSS component bindings");
    }

    sq::ecs::Registry& registry = engine.registry();
    _arena = registry.spawnEntity();
    registry.addComponent(_arena, sq::ecs::Tag{.name = std::string(game::ARENA_TAG)});
    registry.addComponent(_arena, game::ArenaShape{
                                      .left = arena::LEFT,
                                      .top = arena::TOP,
                                      .right = arena::RIGHT,
                                      .bottom = arena::BOTTOM,
                                      .cornerRadius = arena::CORNER_RADIUS,
                                      .goalTop = arena::GOAL_TOP,
                                      .goalBottom = arena::GOAL_BOTTOM,
                                      .goalDepth = arena::GOAL_DEPTH,
                                      .goalCornerRadius = arena::GOAL_CORNER_RADIUS,
                                  });
    registry.addComponent(_arena, game::MatchState{});
    // The match script steps every car and the ball together.
    registry.addComponent(_arena, sq::scripting::ScriptComponent{
                                      .moduleName = std::string(game::MATCH_SCRIPT_MODULE),
                                      .className = "Match",
                                      .hasUpdate = true,
                                  });

    // The ball starts in the middle of the arena and falls.
    _ball = registry.spawnEntity();
    registry.addComponent(_ball, sq::ecs::Transform{
                                     .x = (arena::LEFT + arena::RIGHT) / 2.0F,
                                     .y = (arena::TOP + arena::BOTTOM) / 2.0F,
                                 });
    registry.addComponent(_ball, sq::scripting::ScriptComponent{
                                     .moduleName = std::string(game::BALL_SCRIPT_MODULE),
                                     .className = "Ball",
                                 });
    registry.addComponent(_ball, game::ShotGlow{});
    registry.addComponent(_ball, game::BallCues{});
    sq::net::replicate(registry, _ball, game::KIND_BALL);

    // A player who joins gets the score at once; afterwards, `update` sends it when it changes.
    _connected =
        engine.eventBus().subscribe<sq::net::PeerConnected>([this, &engine](const sq::net::PeerConnected& event) {
            if (const auto status = currentStatus(engine)) {
                engine.service<sq::net::INetworkService>().send(event.peer, protocol::encode(*status),
                                                                sq::net::Channel::Reliable);
            }
        });

    _inputs = engine.eventBus().subscribe<sq::net::InputApplied>([&registry](const sq::net::InputApplied& event) {
        auto& states = registry.getComponents<sq::input::InputState>();
        if (!states.contains(event.entity)) {
            return;
        }
        sq::input::InputState& state = states.get(event.entity);
        float steer = 0.0F;
        float pitch = 0.0F;
        for (const sq::ecs::NetAxis& axis : event.input.axes) {
            if (axis.action == game::STEER_ID) {
                steer = std::clamp(axis.value, -1.0F, 1.0F);
            } else if (axis.action == game::PITCH_ID) {
                pitch = std::clamp(axis.value, -1.0F, 1.0F);
            }
        }
        state.setAxis(game::STEER_ID, steer);
        state.setAxis(game::PITCH_ID, pitch);
        const auto held = [&event](const sq::input::ActionId action) {
            return std::ranges::find(event.input.held, action) != event.input.held.end();
        };
        state.setDown(game::JUMP_ID, held(game::JUMP_ID));
        state.setDown(game::BOOST_ID, held(game::BOOST_ID));
    });
}

void CarSimulationPlugin::update(sq::engine::Engine& engine, const sq::time::Time& /*time*/) {
    const auto status = currentStatus(engine);
    if (!status || (_sent && *status == *_sent)) {
        return;
    }
    engine.service<sq::net::INetworkService>().broadcast(protocol::encode(*status), sq::net::Channel::Reliable);
    _sent = status;
}

std::optional<protocol::MatchStatus> CarSimulationPlugin::currentStatus(sq::engine::Engine& engine) const {
    const auto& states = engine.registry().getComponents<game::MatchState>();
    if (!states.contains(_arena)) {
        return std::nullopt;
    }
    const game::MatchState& state = states.get(_arena);
    const auto score = [](const int goals) {
        return static_cast<std::uint16_t>(
            std::clamp(goals, 0, static_cast<int>(std::numeric_limits<std::uint16_t>::max())));
    };
    return protocol::MatchStatus{
        .blueScore = score(state.blueScore),
        .orangeScore = score(state.orangeScore),
        .phase = static_cast<protocol::MatchPhase>(
            std::clamp(state.phase, 0, static_cast<int>(protocol::MatchPhase::Ended))),
        .countdown =
            static_cast<std::uint8_t>(std::clamp(state.countdown, 0, static_cast<int>(protocol::MAX_COUNTDOWN))),
        .clock = static_cast<std::uint16_t>(std::clamp(state.clock, 0, static_cast<int>(protocol::MAX_CLOCK))),
        .overtime = state.overtime,
    };
}

void CarSimulationPlugin::shutdown(sq::engine::Engine& engine) {
    if (_inputs) {
        engine.eventBus().unsubscribe<sq::net::InputApplied>(*_inputs);
        _inputs.reset();
    }
    if (_connected) {
        engine.eventBus().unsubscribe<sq::net::PeerConnected>(*_connected);
        _connected.reset();
    }
    for (const sq::ecs::Entity entity : {_ball, _arena}) {
        if (engine.registry().isAlive(entity)) {
            engine.registry().killEntity(entity);
        }
    }
}

}  // namespace rlss::server
