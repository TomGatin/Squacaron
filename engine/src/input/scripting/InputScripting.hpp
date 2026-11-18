/**
 * @file InputScripting.hpp
 * @brief Bridge between the input module and Wren: the `core/Input` module.
 */

#pragma once

#include <string_view>
#include <vector>

#include "core/IPlugin.hpp"
#include "scripting/IScriptExtension.hpp"

namespace sq::scripting {

class ScriptSystem;
class WrenVM;

}  // namespace sq::scripting

namespace sq::input {

/**
 * @brief The `core/Input` Wren module: `Input.isDown(action)`, `isJustPressed`, `isJustReleased`, `getAxis`,
 * `mouseX`, `mouseY`, `mouseWheel`, each also with a player index first. It reads the `InputState` of the player.
 */
class InputScriptExtension final : public sq::scripting::IScriptExtension {
  public:
    /** @brief The `core/Input` module. @return One module. */
    [[nodiscard]] std::vector<sq::scripting::ModuleSource> modules() const override;
    /** @brief Binds the foreign methods of `core/Input`. @param vm VM to bind. */
    void bind(sq::scripting::WrenVM& vm) override;
};

/**
 * @brief Makes `core/Input` importable by the scripts of a script system (once per system).
 * @param scripts The script system.
 */
void addInputScripting(sq::scripting::ScriptSystem& scripts);

/** @brief Plugin form of `addInputScripting`, for the script system of an engine. */
class InputScriptingPlugin final : public sq::engine::IPlugin {
  public:
    /** @brief Gets the plugin name. @return "InputScriptingPlugin". */
    [[nodiscard]] std::string_view name() const noexcept override { return "InputScriptingPlugin"; }
    /** @brief Requires the `ScriptPlugin`. @param builder Dependency builder. */
    void build(sq::engine::PluginBuilder& builder) override;
    /** @brief Adds `core/Input` to the script system. @param engine Host engine. */
    void init(sq::engine::Engine& engine) override;
    /** @brief Removes it. @param engine Host engine. */
    void shutdown(sq::engine::Engine& engine) override;

  private:
    /** @brief Script system, between `init` and `shutdown`. */
    sq::scripting::ScriptSystem* _scripts = nullptr;
};

}  // namespace sq::input
