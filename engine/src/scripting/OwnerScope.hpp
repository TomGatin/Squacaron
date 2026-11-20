/**
 * @file OwnerScope.hpp
 * @brief Runs Wren code on behalf of an owner for the length of a scope.
 */

#pragma once

#include "scripting/ScriptOwner.hpp"

namespace scripting {

class ScriptSystem;

/**
 * @brief Makes an owner the owner of the Wren code run in a scope, and restores the previous one at its end.
 *
 * Fibers put to sleep and entities spawned by that code record the owner: an extension runs its scripts (a scene,
 * a console command) inside one, with an id from `makeExtensionOwner()`.
 */
class OwnerScope {
  public:
    /**
     * @brief Sets `owner` as the current owner of `system`.
     * @param system System whose owner changes.
     * @param owner Owner for the scope.
     */
    OwnerScope(ScriptSystem& system, ScriptOwner owner) noexcept;
    /** @brief Restores the previous owner. */
    ~OwnerScope();
    /** @brief Deleted copy constructor. */
    OwnerScope(const OwnerScope&) = delete;
    /** @brief Deleted copy assignment. */
    OwnerScope& operator=(const OwnerScope&) = delete;
    /** @brief Deleted move constructor. */
    OwnerScope(OwnerScope&&) = delete;
    /** @brief Deleted move assignment. */
    OwnerScope& operator=(OwnerScope&&) = delete;

  private:
    /** @brief System whose owner was changed. */
    ScriptSystem& _system;
    /** @brief Owner to restore. */
    ScriptOwner _previous;
};

}  // namespace scripting
