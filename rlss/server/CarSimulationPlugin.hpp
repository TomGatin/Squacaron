/**
 * @file CarSimulationPlugin.hpp
 * @brief Server plugin hosting the match simulation: arena, ball, Wren bindings, player inputs.
 */

#pragma once

#include <optional>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "events/EventBus.hpp"
#include "protocol/Protocol.hpp"

namespace sq::ecs::time {
struct Time;
}

namespace rlss::server {

/**
 * @brief Prepares the world the `Car` Wren scripts simulate, and feeds them the player inputs.
 *
 * - Exposes the RLSS components to Wren (`rlss::game::installScriptBindings`).
 * - Spawns the arena entity (`sq::ecs::Tag` "arena" + `rlss::game::ArenaShape`, read by `Arena.fromWorld()`), which
 * runs the `Match` script, and the replicated ball, driven by the `Ball` script.
 * - On each `sq::net::InputApplied` (one input per car and per fixed step, from `ServerReplicationPlugin`), writes
 *   the input into the car's `sq::input::InputState`, which the scripts read with `Input.getAxis` / `Input.isDown`.
 *
 * - Sends the score, phase and clock that the `Match` script writes in the arena's `rlss::game::MatchState` to the
 *   players (reliable `MatchStatus` message): to everybody when they change, to a player who joins.
 *
 * The physics and the rules are in `rlss/scripts/` (`Match`, `Car`, `Ball`, `Kickoff`).
 */
class CarSimulationPlugin final : public sq::engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "CarSimulationPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "CarSimulationPlugin"; }

    /**
     * @brief Declares the dependencies on `sq::scripting::ScriptPlugin` and `sq::net::ServerReplicationPlugin`.
     * @param builder Dependency builder.
     */
    void build(sq::engine::PluginBuilder& builder) override;

    /**
     * @brief Registers the Wren bindings, spawns the arena and the ball, and subscribes to the applied inputs.
     * @param engine Host engine.
     * @throws std::runtime_error If Wren refuses the bindings.
     */
    void init(sq::engine::Engine& engine) override;

    /**
     * @brief Sends the match status if it changed since the last one sent.
     * @param engine Host engine.
     * @param time Frame time.
     */
    void update(sq::engine::Engine& engine, const sq::time::Time& time) override;

    /**
     * @brief Unsubscribes from the events.
     * @param engine Host engine.
     */
    void shutdown(sq::engine::Engine& engine) override;

    /**
     * @brief Reads the arena's `MatchState` as a message.
     * @param engine Host engine.
     * @return The status, or nothing if the arena has no `MatchState`.
     */
    [[nodiscard]] std::optional<protocol::MatchStatus> currentStatus(sq::engine::Engine& engine) const;

  private:
    /** @brief Arena entity. */
    sq::ecs::Entity _arena = 0;
    /** @brief Ball entity. */
    sq::ecs::Entity _ball = 0;

    /** @brief Subscription to `InputApplied`, while initialized. */
    std::optional<sq::events::EventBus::EventToken> _inputs;
    /** @brief Subscription to `PeerConnected`, while initialized. */
    std::optional<sq::events::EventBus::EventToken> _connected;
    /** @brief Last status sent to everybody. */
    std::optional<protocol::MatchStatus> _sent;
};

}  // namespace rlss::server
