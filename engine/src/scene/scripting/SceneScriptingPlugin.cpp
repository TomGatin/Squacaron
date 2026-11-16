#include "scene/scripting/SceneScriptingPlugin.hpp"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "scene/SceneManager.hpp"
#include "scene/ScenePlugin.hpp"
#include "scene/SceneTypes.hpp"
#include "scene/scripting/SceneScriptModule.hpp"
#include "scene/scripting/ScriptScene.hpp"
#include "scripting/ForeignCall.hpp"
#include "scripting/ScriptPlugin.hpp"
#include "scripting/ScriptSystem.hpp"
#include "scripting/WrenVM.hpp"

namespace sq::scene {

namespace {

/** @brief Name of the Wren class exposing the scene stack. */
constexpr std::string_view SCENE_CLASS = "Scene";

/**
 * @brief Gets the scene manager of the engine running a foreign call.
 * @param call Foreign call.
 * @return The manager, or null outside a running bridge.
 */
SceneManager* managerOf(sq::scripting::ForeignCall& call) {
    auto* scripts = call.userData<sq::scripting::ScriptSystem>();
    if (scripts == nullptr || scripts->getRegistry() == nullptr ||
        !scripts->getRegistry()->hasResource<SceneScriptBridge>()) {
        return nullptr;
    }
    return scripts->getRegistry()->getResource<SceneScriptBridge>().manager;
}

/**
 * @brief Resolves the scene named by the first argument of a foreign call.
 * @param call Foreign call.
 * @param manager Manager to search.
 * @return Its id.
 * @throws std::invalid_argument if the argument is not a string or names no scene (aborts the fiber).
 */
SceneId sceneArgument(sq::scripting::ForeignCall& call, const SceneManager& manager) {
    auto const name = call.arg<std::string_view>(1);
    if (!name) {
        throw std::invalid_argument("Scene: the scene name must be a string");
    }
    const std::optional<SceneId> id = manager.find(*name);
    if (!id) {
        throw std::invalid_argument("Scene: unknown scene '" + std::string(*name) + "'");
    }
    return *id;
}

/**
 * @brief `Scene.switchTo(name)`, `Scene.push(name)` and `Scene.replace(name)`.
 * @tparam Transition Member of `SceneManager` queueing the transition.
 * @param call Foreign call.
 */
template <void (SceneManager::*Transition)(SceneId)>
void sceneTransition(sq::scripting::ForeignCall& call) {
    SceneManager* manager = managerOf(call);
    if (manager != nullptr) {
        (manager->*Transition)(sceneArgument(call, *manager));
    }
}

/**
 * @brief `Scene.pop()`.
 * @param call Foreign call.
 */
void scenePop(sq::scripting::ForeignCall& call) {
    if (SceneManager* manager = managerOf(call); manager != nullptr) {
        manager->pop();
    }
}

/**
 * @brief `Scene.current`: name of the top scene, or null.
 * @param call Foreign call.
 */
void sceneCurrent(sq::scripting::ForeignCall& call) {
    const SceneManager* manager = managerOf(call);
    if (manager == nullptr || manager->top() == INVALID_SCENE) {
        call.retNull();
        return;
    }
    call.ret(std::string(manager->scene(manager->top()).name()));
}

/**
 * @brief `Scene.isActive(name)` and `Scene.isRunning(name)`.
 * @tparam Query Member of `SceneManager` answering the question.
 * @param call Foreign call.
 */
template <bool (SceneManager::*Query)(SceneId) const noexcept>
void sceneQuery(sq::scripting::ForeignCall& call) {
    const SceneManager* manager = managerOf(call);
    call.ret(manager != nullptr && (manager->*Query)(sceneArgument(call, *manager)));
}

/**
 * @brief Binds the foreign methods of `core/Scene`; also replayed on the VM of each hot reload.
 * @param vm VM to bind.
 */
void bindSceneModule(sq::scripting::WrenVM& vm) {
    vm.registerForeignMethod<&sceneTransition<&SceneManager::switchTo>>(SCENE_MODULE_NAME, SCENE_CLASS, true,
                                                                        "switchTo(_)");
    vm.registerForeignMethod<&sceneTransition<&SceneManager::push>>(SCENE_MODULE_NAME, SCENE_CLASS, true, "push(_)");
    vm.registerForeignMethod<&sceneTransition<&SceneManager::replace>>(SCENE_MODULE_NAME, SCENE_CLASS, true,
                                                                       "replace(_)");
    vm.registerForeignMethod<&scenePop>(SCENE_MODULE_NAME, SCENE_CLASS, true, "pop()");
    vm.registerForeignMethod<&sceneCurrent>(SCENE_MODULE_NAME, SCENE_CLASS, true, "current");
    vm.registerForeignMethod<&sceneQuery<&SceneManager::isActive>>(SCENE_MODULE_NAME, SCENE_CLASS, true, "isActive(_)");
    vm.registerForeignMethod<&sceneQuery<&SceneManager::isRunning>>(SCENE_MODULE_NAME, SCENE_CLASS, true,
                                                                    "isRunning(_)");
}

}  // namespace

void SceneScriptingPlugin::build(sq::engine::PluginBuilder& builder) {
    builder.require<ScenePlugin>();
    builder.require<sq::scripting::ScriptPlugin>();
}

void SceneScriptingPlugin::init(sq::engine::Engine& engine) {
    _manager = &engine.getPlugin<ScenePlugin>().manager();
    _scripts = &engine.getPlugin<sq::scripting::ScriptPlugin>().scriptSystem();
    engine.registry().emplaceResource<SceneScriptBridge>(SceneScriptBridge{.manager = _manager});

    _scripts->addExtension(*this);
}

std::vector<sq::scripting::ModuleSource> SceneScriptingPlugin::modules() const {
    return {{.name = std::string(SCENE_MODULE_NAME), .source = std::string(SCENE_MODULE_SOURCE)}};
}

void SceneScriptingPlugin::bind(sq::scripting::WrenVM& vm) { bindSceneModule(vm); }

void SceneScriptingPlugin::onVmReset(const sq::scripting::VmResetPhase phase) {
    for (ScriptScene* scriptScene : _scriptScenes) {
        if (phase == sq::scripting::VmResetPhase::Before) {
            scriptScene->releaseHandles();
        } else {
            scriptScene->rebuildAfterReload();
        }
    }
}

void SceneScriptingPlugin::shutdown(sq::engine::Engine& /*engine*/) {
    if (_scripts != nullptr) {
        _scripts->removeExtension(*this);
    }
    // The manager stays readable until the scenes exit (ScenePlugin shuts down after this plugin).
}

SceneId SceneScriptingPlugin::registerScriptScene(sq::engine::Engine& engine, std::string name, std::string module,
                                                  std::string className, ScriptSceneOptions options) {
    auto scriptScene = std::make_unique<ScriptScene>(std::move(name), std::move(module), std::move(className), options,
                                                     engine.getPlugin<sq::scripting::ScriptPlugin>().scriptSystem());
    SceneManager& manager = engine.getPlugin<ScenePlugin>().manager();
    const SceneId id = manager.add(std::move(scriptScene));
    _scriptScenes.push_back(&dynamic_cast<ScriptScene&>(manager.scene(id)));
    return id;
}

void SceneScriptingPlugin::onScriptSpawn(sq::ecs::Registry& registry, sq::ecs::Entity spawned,
                                         sq::scripting::ScriptOwner owner) {
    if (_manager == nullptr) {
        return;
    }
    const SceneManager& manager = *_manager;

    std::optional<SceneId> scene = sceneOfOwner(owner);
    if (!scene && sq::scripting::isEntityOwner(owner)) {
        const auto entity = static_cast<sq::ecs::Entity>(owner);
        if (const auto* members = static_cast<const sq::ecs::Registry&>(registry).findComponents<SceneMember>();
            members != nullptr && members->contains(entity)) {
            scene = members->get(entity).id;
        }
    }
    if (!scene) {
        const SceneId target = manager.spawnTarget();
        if (target == INVALID_SCENE) {
            return;
        }
        scene = target;
    }
    if (!manager.isActive(*scene)) {
        // Its scene exited between the request and the flush: the entity would never be cleaned up.
        registry.killEntity(spawned);
        return;
    }
    registry.addComponent(spawned, SceneMember{.id = *scene});
}

}  // namespace sq::scene
