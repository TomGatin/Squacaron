#pragma once

#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "metrics/MetricsBoard.hpp"

namespace sq::ecs::time {
struct Time;
}

namespace client {

/**
 * @brief Adds the `Position` line of the local player's ship to the `sq::metrics::MetricsBoard`.
 *
 * The ship is the `sq::ecs::Predicted` entity, so the line appears once the game has started. This is also the example
 * of a game adding its own line to the board.
 */
class ShipMetricsPlugin final : public sq::engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "ShipMetricsPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "ShipMetricsPlugin"; }

    /**
     * @brief Declares the dependency on `sq::metrics::MetricsPlugin`, which creates the board.
     * @param builder Dependency builder.
     */
    void build(sq::engine::PluginBuilder& builder) override;

    /**
     * @brief Writes the position of the ship to the board every `sq::metrics::REFRESH_PERIOD` seconds.
     * @param engine Host engine.
     * @param time Frame time.
     */
    void update(sq::engine::Engine& engine, const sq::time::Time& time) override;

  private:
    /** @brief Paces the refreshes. */
    sq::metrics::RefreshTimer _timer;
};

}  // namespace client
