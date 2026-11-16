/**
 * @file ClockTick.hpp
 * @brief Runs the fixed steps of a clock with the `Time` resource of a registry kept up to date.
 */

#pragma once
#include <cstddef>

#include "ecs/Registry.hpp"
#include "ecs/time/Clock.hpp"
#include "ecs/time/Time.hpp"

namespace ecs::time {

/**
 * @brief Advances a clock, storing the `Time` of each fixed step in `registry` before running it.
 *
 * Calls `step(registry)` once per whole `fixedDt` contained in the accumulated time.
 * @param clock Clock to advance.
 * @param fixedDt Duration of one step in seconds (must be > 0).
 * @param registry Registry receiving the `ecs::time::Time` resource.
 * @param step Callable with signature `void(Registry&)`.
 * @return Number of steps executed.
 */
template <typename Step>
std::size_t tick(Clock& clock, float fixedDt, Registry& registry, Step step) {
    return clock.tick(fixedDt, [&registry, &step](const Time& timeData) {
        if (!registry.hasResource<Time>()) {
            registry.emplaceResource<Time>(timeData);
        } else {
            registry.getResource<Time>() = timeData;
        }
        step(registry);
    });
}

}  // namespace ecs::time
