#include "systems/input/InputSystem.hpp"

#include <utility>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "input/ActionId.hpp"
#include "input/ActionMapping.hpp"
#include "input/InputActionEvent.hpp"
#include "input/InputState.hpp"
#include "render/api/IWindowService.hpp"
#include "render/api/InputCodes.hpp"

namespace sq::systems {

InputSystem::InputSystem(sq::input::ActionMapping mapping, int playerIndex, int gamepadIndex)
    : _mapping(std::move(mapping)), _playerIndex(playerIndex), _gamepadIndex(gamepadIndex) {}

void InputSystem::update(const sq::engine::IWindowService& window, sq::ecs::Registry& registry,
                         sq::events::EventBus* eventBus) {
    registry.runSystem<sq::input::InputState>(
        [this, &window, eventBus](sq::ecs::Entity /*entity*/, sq::input::InputState& state) {
            if (state.playerIndex == _playerIndex) {
                updateState(window, state, eventBus);
            }
        });
}

void InputSystem::updateState(const sq::engine::IWindowService& window, sq::input::InputState& state,
                              sq::events::EventBus* eventBus) {
    state.resetFrameTransitions();

    const sq::engine::MousePosition mousePosition = window.mousePosition();
    state.mouse = sq::input::MouseState{.x = mousePosition.x, .y = mousePosition.y, .wheel = window.mouseWheelDelta()};

    for (const auto& [actionId, binding] : _mapping.bindings()) {
        if (binding.type == sq::input::ActionType::Button) {
            const bool isDown = _mapping.isActionDown(window, actionId, _gamepadIndex);
            auto& actionData = state.actions[actionId];
            const bool wasDown = actionData.held;

            actionData.held = isDown;

            if (!wasDown && isDown) {
                actionData.justPressed = true;
                if (eventBus != nullptr) {
                    eventBus->publish(sq::events::InputActionEvent{
                        .action = actionId,
                        .state = sq::input::ActionState::Pressed,
                        .value = 1.0F,
                        .playerIndex = _playerIndex,
                    });
                }
            } else if (wasDown && !isDown) {
                actionData.justReleased = true;
                if (eventBus != nullptr) {
                    eventBus->publish(sq::events::InputActionEvent{
                        .action = actionId,
                        .state = sq::input::ActionState::Released,
                        .value = 0.0F,
                        .playerIndex = _playerIndex,
                    });
                }
            }
        } else if (binding.type == sq::input::ActionType::Axis) {
            const float axisValue = _mapping.getAxis(window, actionId, _gamepadIndex);
            state.actions[actionId].axis = axisValue;
        }
    }
}

}  // namespace sq::systems
