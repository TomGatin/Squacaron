/**
 * @file PhysicsScriptingPlugin.hpp
 * @brief Bridge between physics and Wren: contacts reach `onCollision(other)` of entity scripts.
 */

#pragma once

#include <string_view>

#include "core/IPlugin.hpp"

namespace sq::scripting {

class ScriptSystem;

}  // namespace sq::scripting

namespace sq::systems {

/**
 * @brief Forwards `sq::events::CollisionBeginEvent` to `onCollision(other)` of the scripts that set
 * `ScriptComponent::hasCollision`. Without it, physics and scripting ignore each other.
 */
class PhysicsScriptingPlugin final : public sq::engine::IPlugin {
  public:
    /** @brief Gets the plugin name. @return "PhysicsScriptingPlugin". */
    [[nodiscard]] std::string_view name() const noexcept override { return "PhysicsScriptingPlugin"; }
    /** @brief Requires the `ScriptPlugin`. @param builder Dependency builder. */
    void build(sq::engine::PluginBuilder& builder) override;
    /** @brief Subscribes the script system to the contacts. @param engine Host engine. */
    void init(sq::engine::Engine& engine) override;
    /** @brief Unsubscribes it. @param engine Host engine. */
    void shutdown(sq::engine::Engine& engine) override;

  private:
    /** @brief Script system, between `init` and `shutdown`. */
    sq::scripting::ScriptSystem* _scripts = nullptr;
};

}  // namespace sq::systems
