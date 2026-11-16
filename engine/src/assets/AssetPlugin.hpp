/**
 * @file AssetPlugin.hpp
 * @brief Engine plugin owning the `AssetManager` registry resource.
 */

#pragma once

#include <filesystem>
#include <string_view>
#include <utility>

#include "core/IPlugin.hpp"

namespace sq::assets {

/**
 * @brief Adds an `AssetManager` to the registry while the engine runs.
 *
 * The loaders are plugged in by the bridges `sq::assets::RenderAssetsPlugin` (`engine_assets_render`) and
 * `sq::assets::AudioAssetsPlugin` (`engine_assets_audio`), which depend on this plugin and on the backend's: they start
 * after both and detach their loaders (releasing their assets) when they shut down, before either of them.
 * Headless: depends only on `engine_core`.
 */
class AssetPlugin final : public sq::engine::IPlugin {
  public:
    /**
     * @brief Creates the plugin.
     * @param root Directory the manager loads relative paths from, usually `sq::engine::contentRoot()`; empty for the
     * working directory.
     */
    explicit AssetPlugin(std::filesystem::path root = {}) : _root(std::move(root)) {}

    /**
     * @brief Gets plugin name.
     * @return String view "AssetPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "AssetPlugin"; }

    /**
     * @brief Adds the `AssetManager` resource.
     * @param engine Host engine.
     */
    void init(sq::engine::Engine& engine) override;

    /**
     * @brief Releases what is still loaded and removes the resource.
     * @param engine Host engine.
     */
    void shutdown(sq::engine::Engine& engine) override;

  private:
    /** @brief Content root given to the manager. */
    std::filesystem::path _root;
};

}  // namespace sq::assets
