/*
** EPITECH PROJECT, 2026
** Squacaron
** File description:
** Plugin wrapping AnimationSystem and ParallaxSystem into the engine lifecycle.
*/

#pragma once

#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "systems/render/ParallaxSystem.hpp"

namespace sq::systems {

/**
 * @brief Engine plugin driving frame-based sprite animation and parallax scrolling.
 *
 * Runs AnimationSystem and ParallaxSystem on every fixed-timestep tick, after physics.
 * Requires IRenderService (window + texture store must be alive).
 */
class AnimationPlugin final : public sq::engine::IPlugin {
  public:
    /** @brief Constructs AnimationPlugin. */
    AnimationPlugin() = default;

    /** @brief Destructor. */
    ~AnimationPlugin() override = default;

    /** @brief Deleted copy constructor. */
    AnimationPlugin(const AnimationPlugin&) = delete;

    /** @brief Deleted copy assignment. */
    AnimationPlugin& operator=(const AnimationPlugin&) = delete;

    /** @brief Deleted move constructor. */
    AnimationPlugin(AnimationPlugin&&) = delete;

    /** @brief Deleted move assignment. */
    AnimationPlugin& operator=(AnimationPlugin&&) = delete;

    /**
     * @brief Gets plugin name.
     * @return String view "AnimationPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "AnimationPlugin"; }

    /**
     * @brief Declares service dependency on IRenderService.
     * @param builder Dependency builder.
     */
    void build(sq::engine::PluginBuilder& builder) override;

    /**
     * @brief Advances animations and parallax scrolling each fixed tick.
     * @param engine Reference to host Engine.
     * @param time Fixed simulation timing.
     */
    void fixedUpdate(sq::engine::Engine& engine, const sq::time::Time& time) override;

  private:
    /** @brief Stateful parallax scroll system tracking camera displacement. */
    ParallaxSystem _parallaxSystem;
};

}  // namespace sq::systems
