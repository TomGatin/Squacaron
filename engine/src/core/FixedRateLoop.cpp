#include "core/FixedRateLoop.hpp"

#include <chrono>
#include <functional>
#include <optional>
#include <thread>

#include "core/platform/StopSignals.hpp"
#include "ecs/time/PreciseSleep.hpp"

namespace sq::engine {

FixedRateLoop::FixedRateLoop(const double ticksPerSecond) noexcept {
    if (ticksPerSecond > 0.0) {
        _period =
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(1.0 / ticksPerSecond));
    }
}

bool FixedRateLoop::stopRequested() const noexcept {
    return _stopRequested.load(std::memory_order_relaxed) || (_stopOnSignals && platform::stopSignalReceived());
}

void FixedRateLoop::run(const std::function<void()>& tick) {
    using Clock = std::chrono::steady_clock;

    if (_stopOnSignals) {
        platform::installStopSignalHandlers();
    }
    const bool paced = _period > std::chrono::nanoseconds::zero();
    // Only a paced loop sleeps: an unpaced one has nothing to gain from a finer system timer.
    std::optional<sq::time::PreciseSleepScope> preciseSleep;
    if (paced) {
        preciseSleep.emplace();
    }

    auto next = Clock::now();
    while (!stopRequested()) {
        tick();
        if (!paced) {
            continue;
        }
        next += _period;
        const auto now = Clock::now();
        if (next > now) {
            std::this_thread::sleep_until(next);
        } else {
            next = now;  // late: no burst to catch up
        }
    }
}

}  // namespace sq::engine
