/**
 * @file InputScripting.cpp
 * @brief The `core/Input` Wren module: actions, axes and mouse of a player.
 */

#include "input/scripting/InputScripting.hpp"

#include <string>
#include <string_view>
#include <vector>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Registry.hpp"
#include "input/InputState.hpp"
#include "scripting/ForeignCall.hpp"
#include "scripting/ScriptPlugin.hpp"
#include "scripting/ScriptSystem.hpp"
#include "scripting/WrenVM.hpp"

namespace sq::input {

namespace {

using sq::scripting::ForeignCall;
using sq::scripting::ScriptSystem;

/**
 * @brief Core Wren script source code for the `core/Input` module.
 *
 * Exposes static query methods for action buttons and continuous axes (`isDown`, `isJustPressed`, `isJustReleased`,
 * `getAxis`), and for the mouse (`mouseX`, `mouseY`, `mouseWheel`).
 */
constexpr std::string_view CORE_INPUT_SOURCE = R"wren(
class Input {
    foreign static isDown(playerIndex, action)
    foreign static isJustPressed(playerIndex, action)
    foreign static isJustReleased(playerIndex, action)
    foreign static getAxis(playerIndex, action)
    foreign static mouseX(playerIndex)
    foreign static mouseY(playerIndex)
    foreign static mouseWheel(playerIndex)

    static isDown(action) { isDown(0, action) }
    static isJustPressed(action) { isJustPressed(0, action) }
    static isJustReleased(action) { isJustReleased(0, action) }
    static getAxis(action) { getAxis(0, action) }
    static mouseX { mouseX(0) }
    static mouseY { mouseY(0) }
    static mouseWheel { mouseWheel(0) }
}
)wren";

/**
 * @brief Resolves the InputState component matching playerIndex.
 * @param registry Active ECS registry.
 * @param playerIndex Logical player identifier.
 * @return Pointer to InputState or nullptr if missing.
 */
const InputState* resolvePlayerInputState(sq::ecs::Registry* registry, int playerIndex) {
    if (registry == nullptr) {
        return nullptr;
    }
    const auto& denseStates = registry->getComponents<InputState>().getDense();
    for (const auto& state : denseStates) {
        if (state.playerIndex == playerIndex) {
            return &state;
        }
    }
    return nullptr;
}

/**
 * @brief Foreign static method `Input.isDown(playerIndex, action)` callback.
 * @param call ForeignCall execution context.
 */
void inputIsDown(ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    if (scriptSystem == nullptr) {
        call.ret(false);
        return;
    }
    auto const playerIndex = call.arg<int>(1);
    auto const actionName = call.arg<std::string_view>(2);
    if (!playerIndex.has_value() || !actionName.has_value()) {
        call.ret(false);
        return;
    }
    const auto* inputState = resolvePlayerInputState(scriptSystem->getRegistry(), *playerIndex);
    call.ret(inputState != nullptr && inputState->isDown(*actionName));
}

/**
 * @brief Foreign static method `Input.isJustPressed(playerIndex, action)` callback.
 * @param call ForeignCall execution context.
 */
void inputIsJustPressed(ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    if (scriptSystem == nullptr) {
        call.ret(false);
        return;
    }
    auto const playerIndex = call.arg<int>(1);
    auto const actionName = call.arg<std::string_view>(2);
    if (!playerIndex.has_value() || !actionName.has_value()) {
        call.ret(false);
        return;
    }
    const auto* inputState = resolvePlayerInputState(scriptSystem->getRegistry(), *playerIndex);
    call.ret(inputState != nullptr && inputState->isJustPressed(*actionName));
}

/**
 * @brief Foreign static method `Input.isJustReleased(playerIndex, action)` callback.
 * @param call ForeignCall execution context.
 */
void inputIsJustReleased(ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    if (scriptSystem == nullptr) {
        call.ret(false);
        return;
    }
    auto const playerIndex = call.arg<int>(1);
    auto const actionName = call.arg<std::string_view>(2);
    if (!playerIndex.has_value() || !actionName.has_value()) {
        call.ret(false);
        return;
    }
    const auto* inputState = resolvePlayerInputState(scriptSystem->getRegistry(), *playerIndex);
    call.ret(inputState != nullptr && inputState->isJustReleased(*actionName));
}

/**
 * @brief Foreign static method `Input.getAxis(playerIndex, action)` callback.
 * @param call ForeignCall execution context.
 */
void inputGetAxis(ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    if (scriptSystem == nullptr) {
        call.ret(0.0F);
        return;
    }
    auto const playerIndex = call.arg<int>(1);
    auto const actionName = call.arg<std::string_view>(2);
    if (!playerIndex.has_value() || !actionName.has_value()) {
        call.ret(0.0F);
        return;
    }
    const auto* inputState = resolvePlayerInputState(scriptSystem->getRegistry(), *playerIndex);
    call.ret(inputState != nullptr ? inputState->getAxis(*actionName) : 0.0F);
}

/**
 * @brief Returns one field of a player's mouse state to Wren.
 * @tparam Field MouseState field to return.
 * @param call ForeignCall execution context.
 */
template <float MouseState::* Field>
void inputMouseField(ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    if (scriptSystem == nullptr) {
        call.ret(0.0F);
        return;
    }
    auto const playerIndex = call.arg<int>(1);
    if (!playerIndex.has_value()) {
        call.ret(0.0F);
        return;
    }
    const auto* inputState = resolvePlayerInputState(scriptSystem->getRegistry(), *playerIndex);
    call.ret(inputState != nullptr ? inputState->mouse.*Field : 0.0F);
}

}  // namespace

std::vector<sq::scripting::ModuleSource> InputScriptExtension::modules() const {
    return {{.name = "core/Input", .source = std::string(CORE_INPUT_SOURCE)}};
}

void InputScriptExtension::bind(sq::scripting::WrenVM& vm) {
    vm.registerForeignMethod<&inputIsDown>("core/Input", "Input", true, "isDown(_,_)");
    vm.registerForeignMethod<&inputIsJustPressed>("core/Input", "Input", true, "isJustPressed(_,_)");
    vm.registerForeignMethod<&inputIsJustReleased>("core/Input", "Input", true, "isJustReleased(_,_)");
    vm.registerForeignMethod<&inputGetAxis>("core/Input", "Input", true, "getAxis(_,_)");
    vm.registerForeignMethod<&inputMouseField<&MouseState::x>>("core/Input", "Input", true, "mouseX(_)");
    vm.registerForeignMethod<&inputMouseField<&MouseState::y>>("core/Input", "Input", true, "mouseY(_)");
    vm.registerForeignMethod<&inputMouseField<&MouseState::wheel>>("core/Input", "Input", true, "mouseWheel(_)");
}

void addInputScripting(sq::scripting::ScriptSystem& scripts) {
    // Stateless: one instance serves every script system.
    static InputScriptExtension extension;
    scripts.addExtension(extension);
}

void InputScriptingPlugin::build(sq::engine::PluginBuilder& builder) { builder.require<sq::scripting::ScriptPlugin>(); }

void InputScriptingPlugin::init(sq::engine::Engine& engine) {
    _scripts = &engine.getPlugin<sq::scripting::ScriptPlugin>().scriptSystem();
    addInputScripting(*_scripts);
}

void InputScriptingPlugin::shutdown(sq::engine::Engine& /*engine*/) {
    if (auto* extension = _scripts != nullptr ? _scripts->findExtension<InputScriptExtension>() : nullptr) {
        _scripts->removeExtension(*extension);
    }
    _scripts = nullptr;
}

}  // namespace sq::input
