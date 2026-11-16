#pragma once

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "input/InputState.hpp"

namespace client {

/** @brief The client has one local player. */
inline constexpr int LOCAL_PLAYER = 0;

/**
 * @brief Runs `action` on the input state of the local player, or does nothing if the game has none.
 * @param registry Client world.
 * @param action Callable taking the `sq::input::InputState`.
 */
template <typename Action>
void withLocalInput(sq::ecs::Registry& registry, Action action) {
    registry.runSystem<sq::input::InputState>([&](sq::ecs::Entity /*entity*/, const sq::input::InputState& state) {
        if (state.playerIndex == LOCAL_PLAYER) {
            action(state);
        }
    });
}

}  // namespace client
