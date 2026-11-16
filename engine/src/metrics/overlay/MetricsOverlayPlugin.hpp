/**
 * @file MetricsOverlayPlugin.hpp
 * @brief Plugin showing the metrics board on a HUD panel.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "metrics/RollingStats.hpp"

namespace sq::time {

struct Time;

}  // namespace sq::time

namespace sq::metrics {

/**
 * @brief Shows the `MetricsBoard` in the bottom-left corner of the screen, with `ui` components.
 *
 * A `sq::ui::UiRect` panel holds a `sq::ui::UiProgressBar` for the frame time (full at 30 FPS) and one `sq::ui::UiText`
 * per metric, coloured by its `MetricLevel`. The text only changes when the board does: a frame without a refresh
 * allocates nothing.
 *
 * `setVisible(false)` destroys the panel and `setVisible(true)` builds it again at the next frame, so a hidden overlay
 * costs nothing. It is shown by default.
 */
class MetricsOverlayPlugin final : public sq::engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "MetricsOverlayPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "MetricsOverlayPlugin"; }

    /**
     * @brief Declares the dependencies on the board, the UI and the hierarchy holding the panel together.
     * @param builder Dependency builder.
     */
    void build(sq::engine::PluginBuilder& builder) override;

    /**
     * @brief Spawns the panel and the frame time bar, if the overlay is visible.
     * @param engine Host engine.
     */
    void init(sq::engine::Engine& engine) override;

    /**
     * @brief Spawns or destroys the panel to match `visible()`, then copies the board to the panel if it changed.
     * @param engine Host engine.
     * @param time Frame time (unused).
     */
    void update(sq::engine::Engine& engine, const sq::time::Time& time) override;

    /**
     * @brief Destroys the panel.
     * @param engine Host engine.
     */
    void shutdown(sq::engine::Engine& engine) override;

    /**
     * @brief Shows or hides the overlay, from the next frame on.
     *
     * Only records the choice, so it can be called at any time, from a menu or a key binding, even before `init`.
     *
     * @param visible `true` to show the overlay, `false` to hide it.
     */
    void setVisible(bool visible) noexcept { _visible = visible; }

    /**
     * @brief Tells whether the overlay is shown.
     * @return The last value given to `setVisible`, `true` by default.
     */
    [[nodiscard]] bool visible() const noexcept { return _visible; }

    /**
     * @brief Gets the panel.
     * @return The entity of the panel, or `sq::ecs::NULL_ENTITY` while the overlay is hidden.
     */
    [[nodiscard]] sq::ecs::Entity panel() const noexcept { return _panel; }

    /**
     * @brief Gets the frame time bar.
     * @return The entity of the bar, once initialized.
     */
    [[nodiscard]] sq::ecs::Entity frameBar() const noexcept { return _frameBar; }

    /**
     * @brief Gets the text lines.
     * @return One entity per metric of the board, in the same order.
     */
    [[nodiscard]] const std::vector<sq::ecs::Entity>& lines() const noexcept { return _lines; }

  private:
    /**
     * @brief Spawns the panel and the frame time bar; the lines come with the next change of the board.
     * @param registry Registry of the panel.
     */
    void spawnPanel(sq::ecs::Registry& registry);
    /**
     * @brief Destroys the panel, with the bar and the lines it holds.
     * @param registry Registry of the panel.
     */
    void destroyPanel(sq::ecs::Registry& registry);
    /**
     * @brief Spawns the text lines missing for the board, and makes the panel tall enough for all of them.
     * @param registry Registry of the panel.
     * @param count Number of metrics on the board.
     */
    void growTo(sq::ecs::Registry& registry, std::size_t count);
    /**
     * @brief Shows the average frame time on the bar.
     * @param registry Registry of the panel.
     * @param frames Last frame times, in seconds.
     */
    void showFrameTime(sq::ecs::Registry& registry, const RollingStats& frames) const;

    /** @brief Background of the overlay, parent of everything else. */
    sq::ecs::Entity _panel = sq::ecs::NULL_ENTITY;
    /** @brief Frame time bar. */
    sq::ecs::Entity _frameBar = sq::ecs::NULL_ENTITY;
    /** @brief One text per metric of the board. */
    std::vector<sq::ecs::Entity> _lines;
    /** @brief Version of the board currently shown. */
    std::uint64_t _shownVersion = 0;
    /** @brief Whether the overlay must be shown. */
    bool _visible = true;
};

}  // namespace sq::metrics
