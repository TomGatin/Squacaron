/**
 * @file PrefabScripting.hpp
 * @brief Bridge between the prefabs and Wren: `Engine.spawnPrefab(name, x, y)` and spawns from game bindings.
 */

#pragma once

#include <optional>
#include <string_view>

#include "core/IPlugin.hpp"
#include "prefabs/PrefabParams.hpp"
#include "scripting/IScriptExtension.hpp"

namespace sq::scripting {

class ScriptSystem;

}  // namespace sq::scripting

namespace sq::prefabs {

class PrefabManager;

/**
 * @brief Spawns prefabs for the Wren code of a script system.
 *
 * Answers `Engine.spawnPrefab(name, x, y)`, and `spawn()` serves the foreign methods of a game that spawn with more
 * parameters. Spawns go through the script system's command buffer, so they are applied at the end of its update,
 * and every extension (scenes) sees the new entity with the owner that asked for it.
 */
class ScriptPrefabSpawner final : public sq::scripting::IScriptExtension {
  public:
    /**
     * @brief Creates a spawner; add it with `ScriptSystem::addExtension`.
     * @param scripts Script system whose code spawns; must outlive the spawner.
     * @param prefabs Prefabs to spawn from; must outlive the spawner.
     */
    ScriptPrefabSpawner(sq::scripting::ScriptSystem& scripts, const PrefabManager& prefabs) noexcept
        : _scripts(&scripts), _prefabs(&prefabs) {}

    /**
     * @brief Spawns a prefab at a position, for `Engine.spawnPrefab`.
     * @param name Prefab name.
     * @param x Horizontal position.
     * @param y Vertical position.
     * @return Always true: an unknown name is answered here (nothing is spawned).
     */
    bool spawnPrefab(std::string_view name, double x, double y) override;

    /**
     * @brief Spawns a prefab with parameters, for the Wren code running now.
     * @param name Prefab name.
     * @param params Spawn parameters.
     * @return False if the prefab is unknown.
     */
    bool spawn(std::string_view name, const PrefabParams& params);

    /**
     * @brief Finds the spawner of a script system, from a foreign method.
     * @param scripts Script system.
     * @return Its spawner, or nullptr if none was added.
     */
    [[nodiscard]] static ScriptPrefabSpawner* of(sq::scripting::ScriptSystem& scripts) noexcept;

  private:
    /** @brief Script system whose code spawns. */
    sq::scripting::ScriptSystem* _scripts;
    /** @brief Prefabs to spawn from. */
    const PrefabManager* _prefabs;
};

/** @brief Plugin form: a `ScriptPrefabSpawner` for the engine's script system and `PrefabPlugin` manager. */
class PrefabScriptingPlugin final : public sq::engine::IPlugin {
  public:
    /** @brief Gets the plugin name. @return "PrefabScriptingPlugin". */
    [[nodiscard]] std::string_view name() const noexcept override { return "PrefabScriptingPlugin"; }
    /** @brief Requires the `PrefabPlugin` and the `ScriptPlugin`. @param builder Dependency builder. */
    void build(sq::engine::PluginBuilder& builder) override;
    /** @brief Adds the spawner to the script system. @param engine Host engine. */
    void init(sq::engine::Engine& engine) override;
    /** @brief Removes it. @param engine Host engine. */
    void shutdown(sq::engine::Engine& engine) override;

  private:
    /** @brief Script system, between `init` and `shutdown`. */
    sq::scripting::ScriptSystem* _scripts = nullptr;
    /** @brief The spawner, between `init` and `shutdown`. */
    std::optional<ScriptPrefabSpawner> _spawner;
};

}  // namespace sq::prefabs
