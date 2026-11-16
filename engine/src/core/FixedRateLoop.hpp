#pragma once

#include <atomic>
#include <chrono>
#include <functional>

namespace sq::engine {

/**
 * @brief Calls a function at a target rate until it is stopped: the loop of a headless program.
 *
 * Without a vsynced window nothing slows a `while (running) advance();` loop down, so it takes a whole core. This
 * loop sleeps until the next tick is due instead (with a 1 ms system timer on Windows, see
 * `sq::time::PreciseSleepScope`), and can stop on Ctrl+C / SIGTERM.
 *
 * @code
 * sq::engine::FixedRateLoop loop(60.0);
 * loop.stopOnSignals(true);
 * loop.run([&] { server.update(); });
 * @endcode
 *
 * A late tick is not caught up with a burst: the next one is simply due one period later. Catching up with the
 * simulation is the job of the engine clock and its fixed steps.
 */
class FixedRateLoop {
  public:
    /**
     * @brief Creates a stopped loop.
     * @param ticksPerSecond Target rate. 0 (or less) means unpaced: the next tick starts at once, for a loop paced by
     * something else, such as a vsynced window.
     */
    explicit FixedRateLoop(double ticksPerSecond = 0.0) noexcept;

    /**
     * @brief Calls `tick` at the target rate until `stop()` is called (or a stop signal arrives, see
     * `stopOnSignals()`). An exception thrown by `tick` leaves the loop and propagates.
     *
     * Returns at once if `stop()` was already called, even from another thread before this call: a stopped loop
     * stays stopped.
     *
     * @param tick Work of one tick; it may call `stop()`.
     */
    void run(const std::function<void()>& tick);

    /** @brief Ends `run()` after the current tick. Thread-safe and signal-safe. */
    void stop() noexcept { _stopRequested.store(true, std::memory_order_relaxed); }

    /**
     * @brief Whether the loop is asked to stop, by `stop()` or by a stop signal.
     * @return True if `run()` returns after the current tick.
     */
    [[nodiscard]] bool stopRequested() const noexcept;

    /**
     * @brief Makes the loop also stop on SIGINT / SIGTERM (Ctrl+C, closing the console on Windows).
     *
     * Installs the handlers of `platform::installStopSignalHandlers()` at the next `run()`.
     * @param enabled True to stop on signals.
     */
    void stopOnSignals(bool enabled) noexcept { _stopOnSignals = enabled; }

    /**
     * @brief The time between two ticks.
     * @return The period, or zero when unpaced.
     */
    [[nodiscard]] std::chrono::nanoseconds period() const noexcept { return _period; }

  private:
    /** @brief Time between two ticks; zero when unpaced. */
    std::chrono::nanoseconds _period{0};
    /** @brief Set by `stop()`. */
    std::atomic<bool> _stopRequested{false};
    /** @brief Whether a stop signal also ends the loop. */
    bool _stopOnSignals = false;
};

}  // namespace sq::engine
