/**
 * @file PhysicsScriptingPlugin.hpp
 * @brief Bridge between physics and Wren: contacts reach `onCollision(other)` of entity scripts.
 */

#pragma once

#include <string_view>

#include "core/IPlugin.hpp"

namespace scripting {
class ScriptSystem;
}

namespace systems {

/**
 * @brief Forwards `events::CollisionBeginEvent` to `onCollision(other)` of the scripts that set
 * `ScriptComponent::hasCollision`. Without it, physics and scripting ignore each other.
 */
class PhysicsScriptingPlugin final : public engine::IPlugin {
  public:
    /** @brief Gets the plugin name. @return "PhysicsScriptingPlugin". */
    [[nodiscard]] std::string_view name() const noexcept override { return "PhysicsScriptingPlugin"; }
    /** @brief Requires the `ScriptPlugin`. @param builder Dependency builder. */
    void build(engine::PluginBuilder& builder) override;
    /** @brief Subscribes the script system to the contacts. @param engine Host engine. */
    void init(engine::Engine& engine) override;
    /** @brief Unsubscribes it. @param engine Host engine. */
    void shutdown(engine::Engine& engine) override;

  private:
    /** @brief Script system, between `init` and `shutdown`. */
    scripting::ScriptSystem* _scripts = nullptr;
};

}  // namespace systems
