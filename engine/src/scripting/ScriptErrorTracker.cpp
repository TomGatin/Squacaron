/**
 * @file ScriptErrorTracker.cpp
 * @brief Implementation of the script error rate limit and disabling.
 */

#include "scripting/ScriptErrorTracker.hpp"

#include <string>
#include <string_view>

#include "log/Logger.hpp"
#include "scripting/ScriptOwner.hpp"
#include "scripting/detail/LogTag.hpp"
#include "scripting/detail/ScriptSupport.hpp"

namespace sq::scripting {

using detail::LOG_TAG;

bool ScriptErrorTracker::isDisabled(const ScriptOwner owner) const noexcept {
    auto const it = _failures.find(owner);
    return it != _failures.end() && it->second.disabled;
}

void ScriptErrorTracker::recordSuccess(const ScriptOwner owner) {
    auto const it = _failures.find(owner);
    if (it == _failures.end() || it->second.disabled) {
        return;
    }
    // Kept while errors are suppressed, so the rate limit still holds for a script failing every other frame.
    it->second.consecutive = 0;
    if (it->second.suppressed == 0 && _elapsed - it->second.lastLogTime >= SCRIPT_ERROR_LOG_INTERVAL) {
        _failures.erase(it);
    }
}

bool ScriptErrorTracker::recordFailure(const ScriptOwner owner, const std::string_view hook,
                                       const std::string_view error) {
    FailureState& state = _failures[owner];
    if (state.disabled) {
        return false;
    }
    ++state.consecutive;
    bool const isEntity = isEntityOwner(owner);

    if (!state.logged || _elapsed - state.lastLogTime >= SCRIPT_ERROR_LOG_INTERVAL) {
        std::string const who = isEntity ? "entity " + std::to_string(owner) : "script owner " + std::to_string(owner);
        if (state.suppressed > 0) {
            sq::engine::log::warning(LOG_TAG, who, ": ", hook, " failed: ", detail::formatScriptError(error), " (",
                                     state.suppressed, " similar errors not shown)");
        } else {
            sq::engine::log::warning(LOG_TAG, who, ": ", hook, " failed: ", detail::formatScriptError(error));
        }
        state.logged = true;
        state.lastLogTime = _elapsed;
        state.suppressed = 0;
    } else {
        ++state.suppressed;
    }

    if (isEntity && state.consecutive >= MAX_CONSECUTIVE_SCRIPT_FAILURES) {
        state.disabled = true;
        sq::engine::log::error(LOG_TAG, "script of entity ", owner, " disabled after ", state.consecutive,
                               " consecutive failures (enabled again by the next hot reload)");
        return true;
    }
    return false;
}

}  // namespace sq::scripting
