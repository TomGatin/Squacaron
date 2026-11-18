/**
 * @file PrefabPlugin.cpp
 * @brief Implementation of PrefabPlugin.
 */

#include "prefabs/PrefabPlugin.hpp"

#include <memory>

#include "core/Engine.hpp"
#include "core/Mediator.hpp"
#include "core/MediatorError.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "prefabs/PrefabManager.hpp"

namespace sq::prefabs {

PrefabPlugin::PrefabPlugin() : _manager(std::make_shared<PrefabManager>()) {}

PrefabPlugin::~PrefabPlugin() = default;

void PrefabPlugin::build(sq::engine::PluginBuilder& builder) {
    builder.require<sq::engine::CorePlugin>();
    builder.provides<PrefabManager>();
}

void PrefabPlugin::init(sq::engine::Engine& engine) {
    if (_manager != nullptr) {
        if (const auto res = engine.mediator().registerService<PrefabManager>(_manager); !res) {
            throw sq::engine::MediatorError(res.error());
        }
    }
}

void PrefabPlugin::shutdown(sq::engine::Engine& engine) {
    if (engine.mediator().hasService<PrefabManager>()) {
        engine.mediator().removeService<PrefabManager>();
    }
}

}  // namespace sq::prefabs
