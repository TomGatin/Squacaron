#pragma once

#include "ecs/components/Transform.hpp"

namespace sq::ecs {

/**
 * @brief Smooths a replicated entity between two server snapshots (client side).
 *
 * Snapshots arrive at the server tick rate, slower and less regularly than frames. When an entity carries this
 * component, a received position does not overwrite its `Transform`: it becomes the new target, and
 * `sq::net::interpolate()` moves the `Transform` from the previous state to it over the snapshot interval.
 */
struct Interpolated {
    /** @brief State displayed when the last snapshot was received. */
    ecs::Transform from;
    /** @brief State of the last snapshot, reached after `duration` seconds. */
    ecs::Transform to;
    /** @brief Seconds elapsed since the last snapshot. */
    float elapsed = 0.0F;
    /** @brief Seconds between the last two snapshots of this entity; 0 snaps to `to`. */
    float duration = 0.0F;
    /** @brief False until the first snapshot: the entity then appears at its position instead of sliding to it. */
    bool initialized = false;
};

}  // namespace sq::ecs
