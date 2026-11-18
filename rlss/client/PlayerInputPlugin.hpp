/**
 * @file PlayerInputPlugin.hpp
 * @brief Client plugin sending the local player's controls to the server.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "events/EventBus.hpp"

namespace sq::ecs::time {
struct Time;
}

namespace rlss::client {

/**
 * @brief Creates the local player's `sq::input::InputState` (sampled by `sq::input::InputPlugin`) and publishes it as
 * an `sq::net::LocalInput` every fixed step while the player is in a match (between `MatchJoined` and `MatchLeft`);
 * `ClientReplicationPlugin` sends it to the server.
 */
class PlayerInputPlugin final : public sq::engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "PlayerInputPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "PlayerInputPlugin"; }

    /**
     * @brief Declares the dependencies on `sq::input::InputPlugin` and `sq::net::ClientReplicationPlugin`.
     * @param builder Dependency builder.
     */
    void build(sq::engine::PluginBuilder& builder) override;

    /**
     * @brief Spawns the local player's input state.
     * @param engine Host engine.
     */
    void init(sq::engine::Engine& engine) override;

    /**
     * @brief Publishes this step's input.
     * @param engine Host engine.
     * @param time Fixed step timing.
     */
    void fixedUpdate(sq::engine::Engine& engine, const sq::time::Time& time) override;

    /**
     * @brief Unsubscribes from the match events.
     * @param engine Host engine.
     */
    void shutdown(sq::engine::Engine& engine) override;

  private:
    /** @brief Sequence of the last input sent; keeps growing from one match to the next. */
    std::uint32_t _sequence = 0;
    /** @brief True while the player is in a match. */
    bool _inMatch = false;
    /** @brief Subscription to `MatchJoined`, while initialized. */
    std::optional<sq::events::EventBus::EventToken> _joined;
    /** @brief Subscription to `MatchLeft`, while initialized. */
    std::optional<sq::events::EventBus::EventToken> _left;
};

}  // namespace rlss::client
