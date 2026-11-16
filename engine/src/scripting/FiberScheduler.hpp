/**
 * @file FiberScheduler.hpp
 * @brief Wren fibers put to sleep by `Engine.wait(seconds)`, resumed once their delay is over.
 */

#pragma once

#include <cstddef>
#include <functional>
#include <vector>

#include "scripting/ScriptOwner.hpp"
#include "scripting/detail/WrenTypes.hpp"

namespace scripting {

/** @brief A suspended Wren fiber waiting for its delay (`Engine.wait`). */
struct SleepingFiber {
    /** @brief Owning handle to the suspended fiber. */
    detail::UniqueWrenHandle fiber;
    /** @brief Seconds left before it resumes. */
    float timeRemaining = 0.0F;
    /** @brief Owner running when the fiber went to sleep; restored while it runs again. */
    ScriptOwner owner = NO_SCRIPT_OWNER;
};

/** @brief Holds the sleeping fibers and hands back those whose delay is over. */
class FiberScheduler {
  public:
    /** @brief Function resuming one fiber whose delay is over. */
    using Resume = std::function<void(SleepingFiber& fiber)>;

    /**
     * @brief Puts a fiber to sleep.
     * @param fiber Owning handle to the fiber.
     * @param seconds Delay before it resumes.
     * @param owner Owner running now, restored when it resumes.
     */
    void add(detail::UniqueWrenHandle fiber, float seconds, ScriptOwner owner);

    /**
     * @brief Drops the sleeping fibers of an owner, so they never run again.
     * @param owner Owner whose fibers to cancel.
     */
    void cancel(ScriptOwner owner);

    /** @brief Drops every sleeping fiber (their handles point into a VM about to be destroyed). */
    void clear() noexcept { _sleeping.clear(); }

    /**
     * @brief Advances the delays by `dt`, then calls `resume` on every fiber whose delay is over, oldest first.
     *
     * Fibers put to sleep or cancelled by `resume` take effect at once: a new one waits for the next call.
     * @param dt Elapsed seconds.
     * @param resume Function resuming one fiber.
     */
    void update(float dt, const Resume& resume);

    /**
     * @brief Number of sleeping fibers.
     * @return The count.
     */
    [[nodiscard]] std::size_t size() const noexcept { return _sleeping.size(); }

  private:
    /** @brief Fibers waiting for their delay. */
    std::vector<SleepingFiber> _sleeping;
    /** @brief Fibers woken this frame; capacity kept between frames. */
    std::vector<SleepingFiber> _ready;
};

}  // namespace scripting
