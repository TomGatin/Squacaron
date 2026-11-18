/**
 * @file CoreBindings.cpp
 * @brief Foreign methods of the core modules: `Engine.wait`, `Engine.spawnPrefab`, `Entity.destroy`, `World` queries.
 */

#include <string_view>
#include <utility>

#include "ecs/Entity.hpp"
#include "ecs/SpatialQueries.hpp"
#include "scripting/ForeignCall.hpp"
#include "scripting/ScriptSystem.hpp"
#include "scripting/WrenVM.hpp"
#include "scripting/detail/Bindings.hpp"

namespace sq::scripting::detail {

namespace {

/**
 * @brief Foreign static method `Engine.registerWait(fiber, seconds)` callback.
 * @param call ForeignCall execution context.
 */
void engineRegisterWait(ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    if (scriptSystem == nullptr) {
        return;
    }
    auto fiber = call.arg<UniqueWrenHandle>(1);
    auto seconds = call.arg<float>(2);
    if (!fiber.has_value() || !seconds.has_value()) {
        return;
    }
    scriptSystem->addSleepingFiber(std::move(*fiber), *seconds);
}

/**
 * @brief Foreign static method `Engine.spawnPrefab(name, x, y)` callback.
 * @param call ForeignCall execution context.
 */
void engineSpawnPrefab(ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    if (scriptSystem == nullptr) {
        return;
    }
    auto const prefabName = call.arg<std::string_view>(1);
    auto const x = call.arg<double>(2);
    auto const y = call.arg<double>(3);
    if (!prefabName.has_value() || !x.has_value() || !y.has_value()) {
        return;
    }

    (void)scriptSystem->requestPrefabSpawn(*prefabName, *x, *y);
}

/**
 * @brief Foreign static method `Entity.destroyEntity(id)` callback.
 * @param call ForeignCall execution context.
 */
void entityDestroy(ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    if (scriptSystem == nullptr) {
        return;
    }
    auto const e = call.arg<sq::ecs::Entity>(1);
    if (!e.has_value()) {
        return;
    }
    scriptSystem->queueDestroyEntity(*e);
}

/**
 * @brief Returns the query buffer to Wren as a list of entity ids, skipping entities pending destruction.
 * @param call ForeignCall execution context.
 * @param scriptSystem Owner of the buffer and of the pending-destroy queue.
 */
void retEntityIdList(ForeignCall& call, ScriptSystem& scriptSystem) {
    call.retList();
    for (sq::ecs::Entity const e : scriptSystem.getQueryBuffer()) {
        if (!scriptSystem.isPendingDestroy(e)) {
            call.listAppend(static_cast<double>(e));
        }
    }
}

/**
 * @brief Foreign static method `World.idsWithTag_(tag)` callback, backing `World.getEntitiesWithTag`.
 * @param call ForeignCall execution context.
 */
void worldIdsWithTag(ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    auto const tag = call.arg<std::string_view>(1);
    if (!tag.has_value()) {
        return;
    }
    if (scriptSystem == nullptr || scriptSystem->getRegistry() == nullptr) {
        call.retList();
        return;
    }
    sq::ecs::entitiesWithTag(*scriptSystem->getRegistry(), *tag, scriptSystem->getQueryBuffer());
    retEntityIdList(call, *scriptSystem);
}

/**
 * @brief Foreign static method `World.nearestIdWithTag_(x, y, tag)` callback, backing `World.getNearestWithTag`.
 * @param call ForeignCall execution context.
 */
void worldNearestIdWithTag(ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    auto const x = call.arg<float>(1);
    auto const y = call.arg<float>(2);
    auto const tag = call.arg<std::string_view>(3);
    if (!x.has_value() || !y.has_value() || !tag.has_value()) {
        return;
    }
    if (scriptSystem == nullptr || scriptSystem->getRegistry() == nullptr) {
        call.retNull();
        return;
    }
    auto const nearest =
        sq::ecs::nearestWithTag(*scriptSystem->getRegistry(), *x, *y, *tag, scriptSystem->getPendingDestroyEntities());
    if (nearest.has_value()) {
        call.ret(static_cast<double>(*nearest));
    } else {
        call.retNull();
    }
}

/**
 * @brief Foreign static method `World.idsInRadius_(x, y, radius)` callback, backing `World.getEntitiesInRadius`.
 * @param call ForeignCall execution context.
 */
void worldIdsInRadius(ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    auto const x = call.arg<float>(1);
    auto const y = call.arg<float>(2);
    auto const radius = call.arg<float>(3);
    if (!x.has_value() || !y.has_value() || !radius.has_value()) {
        return;
    }
    if (scriptSystem == nullptr || scriptSystem->getRegistry() == nullptr) {
        call.retList();
        return;
    }
    sq::ecs::entitiesInRadius(*scriptSystem->getRegistry(), *x, *y, *radius, scriptSystem->getQueryBuffer());
    retEntityIdList(call, *scriptSystem);
}

}  // namespace

void registerCoreBindings(WrenVM& vm) {
    vm.registerForeignMethod<&engineRegisterWait>("core/Engine", "Engine", true, "registerWait(_,_)");
    vm.registerForeignMethod<&engineSpawnPrefab>("core/Engine", "Engine", true, "spawnPrefab(_,_,_)");
    vm.registerForeignMethod<&entityDestroy>("core/Entity", "Entity", true, "destroyEntity(_)");
    vm.registerForeignMethod<&worldIdsWithTag>("core/World", "World", true, "idsWithTag_(_)");
    vm.registerForeignMethod<&worldNearestIdWithTag>("core/World", "World", true, "nearestIdWithTag_(_,_,_)");
    vm.registerForeignMethod<&worldIdsInRadius>("core/World", "World", true, "idsInRadius_(_,_,_)");
}

}  // namespace sq::scripting::detail
