/**
 * @file AssetScriptingPlugin.hpp
 * @brief Bridge between the asset manager and Wren: the `core/Assets` module and the game's declaration script.
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "scripting/IScriptExtension.hpp"

namespace sq::ecs {

class Registry;

}  // namespace sq::ecs

namespace sq::scripting {

class ScriptSystem;

}  // namespace sq::scripting

namespace sq::assets {

/**
 * @brief Lets Wren scripts declare, load and release assets.
 *
 * - Adds the built-in module `core/Assets` (`Assets.texture(alias, path, group)`, `sound`, `font`, `music`,
 *   `declare(kind, ...)`, `preload*`, `loadGroup`, `loadGroupStep`, `unloadGroup`, `textureId`...), kept across hot
 *   reloads.
 * - Runs the game's declaration module, given to the constructor, at init: game plugins that require this one find
 *   its aliases declared. On a hot reload the modules run again: declaring the same assets again loads nothing, and
 *   a declaration the scripts no longer make is removed (its asset is released if nothing else needs it).
 *
 * Script arguments are untrusted: a value of the wrong type aborts the calling fiber with an error; an invalid path
 * or name, a path leaving the working directory (absolute, or starting with `..`) and an unload of the `"global"`
 * group are rejected with a warning of the manager. This module is the only one that depends on both
 * `engine_assets` and `engine_scripting`.
 */
class AssetScriptingPlugin final : public sq::engine::IPlugin, private sq::scripting::IScriptExtension {
  public:
    /**
     * @brief Creates the bridge.
     * @param declarationModule Wren module declaring the game's assets (`"scripts/rtype/Assets"`), or empty.
     */
    explicit AssetScriptingPlugin(std::string declarationModule = {});

    /**
     * @brief Gets plugin name.
     * @return String view "AssetScriptingPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "AssetScriptingPlugin"; }

    /**
     * @brief Declares the dependencies on `AssetPlugin` and `ScriptPlugin`.
     * @param builder Dependency builder.
     */
    void build(sq::engine::PluginBuilder& builder) override;

    /**
     * @brief Adds `core/Assets`, runs the declaration module and follows the hot reloads.
     * @param engine Host engine.
     */
    void init(sq::engine::Engine& engine) override;

    /**
     * @brief Stops following the hot reloads.
     * @param engine Host engine.
     */
    void shutdown(sq::engine::Engine& engine) override;

    /**
     * @brief Tells whether the declaration module ran without error at init.
     * @return True if it ran, or if there is none.
     */
    [[nodiscard]] bool declarationsLoaded() const noexcept { return _declarationsLoaded; }

  private:
    /** @brief The `core/Assets` module. @return One module. */
    [[nodiscard]] std::vector<sq::scripting::ModuleSource> modules() const override;
    /** @brief Binds the foreign methods of `core/Assets`. @param vm VM to bind. */
    void bind(sq::scripting::WrenVM& vm) override;
    /**
     * @brief Around a hot reload, the modules run their declarations again: those no longer made are dropped at the
     * end, unless a module failed to run again (its missing declarations would unload assets still in use).
     * @param phase Phase.
     */
    void onVmReset(sq::scripting::VmResetPhase phase) override;

    /** @brief Wren module declaring the game's assets, or empty. */
    std::string _declarationModule;
    /** @brief Whether the declaration module ran at init. */
    bool _declarationsLoaded = false;
    /** @brief Script system followed for hot reloads. */
    sq::scripting::ScriptSystem* _scripts = nullptr;
    /** @brief Registry holding the `AssetManager`, between `init` and `shutdown`. */
    sq::ecs::Registry* _registry = nullptr;
};

}  // namespace sq::assets
