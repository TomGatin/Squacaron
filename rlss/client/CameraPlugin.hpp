/**
 * @file CameraPlugin.hpp
 * @brief Client plugin owning the camera, driven by the `CameraDirector` Wren script.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "events/EventBus.hpp"

namespace sq::ecs::time {
struct Time;
}

namespace rlss::client {

/**
 * @brief Spawns the camera entity (`sq::ecs::CameraComponent`, tag "main_camera", `rlss::game::CameraRig`,
 * `CameraDirector` script) and keeps its `CameraRig` up to date: the local car (from the server's `Welcome` and the
 * network id map), the ball (from the replication events), the match phase (from `MatchStatus`), the window size and
 * the arena bounds. The framing itself (aim, zoom, easing, bounds) is in `rlss/scripts/CameraDirector.wren`.
 */
class CameraPlugin final : public sq::engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "CameraPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "CameraPlugin"; }

    /**
     * @brief Declares the dependencies on the render, network, replication plugins and the car view (bindings).
     * @param builder Dependency builder.
     */
    void build(sq::engine::PluginBuilder& builder) override;

    /**
     * @brief Spawns the camera and subscribes to the network and replication events.
     * @param engine Host engine.
     */
    void init(sq::engine::Engine& engine) override;

    /**
     * @brief Resolves the local car from its network id.
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
     * @brief Camera entity, carrying the `sq::ecs::CameraComponent` the world is drawn through.
     * @return The entity (valid once initialized).
     */
    [[nodiscard]] sq::ecs::Entity camera() const noexcept { return _camera; }

  private:
    /** @brief Camera entity. */
    sq::ecs::Entity _camera = 0;
    /** @brief Network id of the local car, once the server said it. */
    std::optional<std::uint32_t> _localCar;
    /** @brief Subscription to `PacketReceived`, while initialized. */
    std::optional<sq::events::EventBus::EventToken> _packets;
    /** @brief Subscription to `ReplicatedEntitySpawned`, while initialized. */
    std::optional<sq::events::EventBus::EventToken> _spawned;
    /** @brief Subscription to `ReplicatedEntityDespawned`, while initialized. */
    std::optional<sq::events::EventBus::EventToken> _despawned;
    /** @brief Subscription to `MatchJoined`, while initialized. */
    std::optional<sq::events::EventBus::EventToken> _joined;
    /** @brief Subscription to `MatchLeft`, while initialized. */
    std::optional<sq::events::EventBus::EventToken> _left;
};

}  // namespace rlss::client
