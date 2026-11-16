#pragma once

#include "input/ActionId.hpp"

namespace sq::events {

/**
 * @brief Event published when an action transitions state.
 *
 * Designed to be serialized over UDP to the server or consumed locally.
 */
struct InputActionEvent {
    /** @brief The logical input action identifier triggered. */
    sq::input::ActionId action = 0;

    /** @brief Transition state of the action. */
    sq::input::ActionState state = sq::input::ActionState::Pressed;

    /** @brief Analog or digital magnitude of the action. */
    float value = 1.0F;

    /** @brief Index of the player originating the action. */
    int playerIndex = 0;
};

}  // namespace sq::events
