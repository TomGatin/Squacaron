#include "hierarchy/HierarchyPlugin.hpp"

#include <memory>

#include "core/Engine.hpp"
#include "ecs/time/Time.hpp"
#include "hierarchy/HierarchyHooks.hpp"
#include "hierarchy/TransformPropagation.hpp"

namespace sq::hierarchy {

HierarchyPlugin::HierarchyPlugin() = default;

HierarchyPlugin::~HierarchyPlugin() = default;

void HierarchyPlugin::init(sq::engine::Engine& engine) { _hooks = std::make_unique<HierarchyHooks>(engine.registry()); }

void HierarchyPlugin::lateUpdate(sq::engine::Engine& engine, const sq::time::Time& /*time*/) {
    propagateTransforms(engine.registry());
}

void HierarchyPlugin::shutdown(sq::engine::Engine& /*engine*/) { _hooks.reset(); }

}  // namespace sq::hierarchy
