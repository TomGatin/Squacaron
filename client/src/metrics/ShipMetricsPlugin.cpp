#include "metrics/ShipMetricsPlugin.hpp"

#include <format>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/time/Time.hpp"
#include "metrics/MetricsBoard.hpp"
#include "metrics/MetricsPlugin.hpp"
#include "net/components/Predicted.hpp"

namespace client {

void ShipMetricsPlugin::build(sq::engine::PluginBuilder& builder) { builder.require<sq::metrics::MetricsPlugin>(); }

void ShipMetricsPlugin::update(sq::engine::Engine& engine, const sq::time::Time& time) {
    if (!_timer.tick(time.deltaTime)) {
        return;
    }
    sq::ecs::Registry& registry = engine.registry();
    auto& board = registry.getResource<sq::metrics::MetricsBoard>();
    registry.runSystem<sq::ecs::Predicted, sq::ecs::Transform>([&board](const sq::ecs::Entity /*entity*/,
                                                                        const sq::ecs::Predicted& /*predicted*/,
                                                                        const sq::ecs::Transform& transform) {
        board.set("Position", std::format("{:.0f}, {:.0f}", transform.x, transform.y));
    });
}

}  // namespace client
