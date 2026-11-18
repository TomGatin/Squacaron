/**
 * @file PrefabScripting.cpp
 * @brief Implementation of the prefab spawner of the scripts.
 */

#include "prefabs/scripting/PrefabScripting.hpp"

#include <string_view>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "prefabs/PrefabManager.hpp"
#include "prefabs/PrefabParams.hpp"
#include "prefabs/PrefabPlugin.hpp"
#include "scripting/ScriptPlugin.hpp"
#include "scripting/ScriptSystem.hpp"

namespace sq::prefabs {

bool ScriptPrefabSpawner::spawnPrefab(std::string_view name, double x, double y) {
    (void)spawn(name, {{"x", x}, {"y", y}});
    return true;
}

bool ScriptPrefabSpawner::spawn(std::string_view name, const PrefabParams& params) {
    return _prefabs->spawn(name, _scripts->getCommandBuffer(), params, _scripts->spawnObserver());
}

ScriptPrefabSpawner* ScriptPrefabSpawner::of(sq::scripting::ScriptSystem& scripts) noexcept {
    return scripts.findExtension<ScriptPrefabSpawner>();
}

void PrefabScriptingPlugin::build(sq::engine::PluginBuilder& builder) {
    builder.require<PrefabPlugin>();
    builder.require<sq::scripting::ScriptPlugin>();
}

void PrefabScriptingPlugin::init(sq::engine::Engine& engine) {
    _scripts = &engine.getPlugin<sq::scripting::ScriptPlugin>().scriptSystem();
    _spawner.emplace(*_scripts, engine.getPlugin<PrefabPlugin>().manager());
    _scripts->addExtension(*_spawner);
}

void PrefabScriptingPlugin::shutdown(sq::engine::Engine& /*engine*/) {
    if (_scripts != nullptr && _spawner) {
        _scripts->removeExtension(*_spawner);
    }
    _spawner.reset();
    _scripts = nullptr;
}

}  // namespace sq::prefabs
