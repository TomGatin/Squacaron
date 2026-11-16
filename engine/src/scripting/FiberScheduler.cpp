/**
 * @file FiberScheduler.cpp
 * @brief Implementation of the sleeping fiber queue.
 */

#include "scripting/FiberScheduler.hpp"

#include <utility>
#include <vector>

#include "scripting/ScriptOwner.hpp"
#include "scripting/detail/WrenTypes.hpp"

namespace sq::scripting {

void FiberScheduler::add(detail::UniqueWrenHandle fiber, const float seconds, const ScriptOwner owner) {
    _sleeping.push_back(SleepingFiber{.fiber = std::move(fiber), .timeRemaining = seconds, .owner = owner});
}

void FiberScheduler::cancel(const ScriptOwner owner) {
    std::erase_if(_sleeping, [owner](const SleepingFiber& sleeping) { return sleeping.owner == owner; });
}

void FiberScheduler::update(const float dt, const Resume& resume) {
    for (auto it = _sleeping.begin(); it != _sleeping.end();) {
        it->timeRemaining -= dt;
        if (it->timeRemaining <= 0.0F) {
            _ready.push_back(std::move(*it));
            it = _sleeping.erase(it);
        } else {
            ++it;
        }
    }
    for (SleepingFiber& fiber : _ready) {
        resume(fiber);
    }
    _ready.clear();
}

}  // namespace sq::scripting
