/**
 * @file InputBindings.cpp
 * @brief Foreign methods of the `core/Input` module: actions, axes and mouse of a player.
 */

#include <string_view>

#include "ecs/Registry.hpp"
#include "input/InputState.hpp"
#include "scripting/ForeignCall.hpp"
#include "scripting/ScriptSystem.hpp"
#include "scripting/WrenVM.hpp"
#include "scripting/detail/Bindings.hpp"

namespace scripting::detail {

namespace {

/**
 * @brief Resolves the InputState component matching playerIndex.
 * @param registry Active ECS registry.
 * @param playerIndex Logical player identifier.
 * @return Pointer to InputState or nullptr if missing.
 */
const input::InputState* resolvePlayerInputState(ecs::Registry* registry, int playerIndex) {
    if (registry == nullptr) {
        return nullptr;
    }
    const auto& denseStates = registry->getComponents<input::InputState>().getDense();
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
template <float input::MouseState::* Field>
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

void registerInputBindings(WrenVM& vm) {
    vm.registerForeignMethod<&inputIsDown>("core/Input", "Input", true, "isDown(_,_)");
    vm.registerForeignMethod<&inputIsJustPressed>("core/Input", "Input", true, "isJustPressed(_,_)");
    vm.registerForeignMethod<&inputIsJustReleased>("core/Input", "Input", true, "isJustReleased(_,_)");
    vm.registerForeignMethod<&inputGetAxis>("core/Input", "Input", true, "getAxis(_,_)");
    vm.registerForeignMethod<&inputMouseField<&input::MouseState::x>>("core/Input", "Input", true, "mouseX(_)");
    vm.registerForeignMethod<&inputMouseField<&input::MouseState::y>>("core/Input", "Input", true, "mouseY(_)");
    vm.registerForeignMethod<&inputMouseField<&input::MouseState::wheel>>("core/Input", "Input", true, "mouseWheel(_)");
}

}  // namespace scripting::detail
