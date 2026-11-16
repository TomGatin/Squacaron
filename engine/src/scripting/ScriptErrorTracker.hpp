/**
 * @file ScriptErrorTracker.hpp
 * @brief Rate-limited logging of failed script calls, and disabling of scripts that keep failing.
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "scripting/ScriptOwner.hpp"

namespace sq::scripting {

/** @brief Consecutive failed calls after which the script of an entity is disabled until the next hot reload. */
inline constexpr std::uint32_t MAX_CONSECUTIVE_SCRIPT_FAILURES = 30;

/** @brief Shortest time between two error messages about the same script, in seconds; the others are counted. */
inline constexpr float SCRIPT_ERROR_LOG_INTERVAL = 1.0F;

/**
 * @brief Error state of the scripts, per owner.
 *
 * Logs a failed call with its stack trace, at most once per `SCRIPT_ERROR_LOG_INTERVAL` per owner (the others are
 * counted and reported with the next message), and disables an entity script after
 * `MAX_CONSECUTIVE_SCRIPT_FAILURES` failures in a row. Also remembers which module problems were already logged.
 */
class ScriptErrorTracker {
  public:
    /**
     * @brief Advances the clock of the rate limit.
     * @param dt Elapsed seconds.
     */
    void advance(float dt) noexcept { _elapsed += dt; }

    /**
     * @brief Records a failed call into Wren.
     * @param owner Owner of the failed code (an extension owner is never disabled).
     * @param hook What was called, for the message (e.g. "update(_)").
     * @param error Error text of the VM (`WrenVM::lastError()`).
     * @return True if this failure disabled the script of `owner`.
     */
    bool recordFailure(ScriptOwner owner, std::string_view hook, std::string_view error);

    /**
     * @brief Records a successful call into Wren: resets the failure count of the owner.
     * @param owner Owner of the code.
     */
    void recordSuccess(ScriptOwner owner);

    /**
     * @brief Tells whether the script of an owner was disabled.
     * @param owner Owner to check.
     * @return True if it is disabled.
     */
    [[nodiscard]] bool isDisabled(ScriptOwner owner) const noexcept;

    /**
     * @brief Forgets the error state of an owner (its script is gone).
     * @param owner Owner to forget.
     */
    void forget(ScriptOwner owner) { _failures.erase(owner); }

    /**
     * @brief Tells whether a module problem should be logged: true the first time `key` is seen since `clear()`.
     * Scripts that cannot start are retried every frame, and must not log every frame.
     * @param key What went wrong, e.g. the module name.
     * @return True if it was not reported yet.
     */
    bool reportOnce(std::string key) { return _reportedProblems.insert(std::move(key)).second; }

    /** @brief Gives every script a new chance, after a hot reload: clears the failures and the reported problems. */
    void clear() noexcept {
        _failures.clear();
        _reportedProblems.clear();
    }

  private:
    /** @brief Error state of one owner. */
    struct FailureState {
        /** @brief Failed calls in a row. */
        std::uint32_t consecutive = 0;
        /** @brief Errors not logged since the last message. */
        std::uint32_t suppressed = 0;
        /** @brief Clock (`_elapsed`) at the last message. */
        float lastLogTime = 0.0F;
        /** @brief Whether a message was logged yet. */
        bool logged = false;
        /** @brief Whether the script is disabled. */
        bool disabled = false;
    };

    /** @brief Error state of the owners whose last calls failed (or disabled), by owner. */
    std::unordered_map<ScriptOwner, FailureState> _failures;
    /** @brief Module problems already logged. */
    std::unordered_set<std::string> _reportedProblems;
    /** @brief Clock of the rate limit: sum of the `advance()` times. */
    float _elapsed = 0.0F;
};

}  // namespace sq::scripting
