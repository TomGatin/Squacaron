#include "gameplay/ScriptBindings.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

#include "LogTag.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "gameplay/Targeting.hpp"
#include "hierarchy/Hierarchy.hpp"
#include "log/Logger.hpp"
#include "prefabs/Health.hpp"
#include "prefabs/PrefabManager.hpp"
#include "prefabs/PrefabParams.hpp"
#include "prefabs/boss_part/BossPart.hpp"
#include "prefabs/missile/Missile.hpp"
#include "prefabs/scripting/PrefabScripting.hpp"
#include "scripting/ComponentBinder.hpp"
#include "scripting/ForeignCall.hpp"
#include "scripting/ScriptSystem.hpp"
#include "scripting/WrenVM.hpp"

namespace rtype::server::gameplay {

namespace {

/**
 * @brief Binds `sq::ecs::Transform` as `Position` in one VM.
 * @param vm Wren VM.
 * @return True if the generated class was interpreted.
 */
bool bindPosition(sq::scripting::WrenVM& vm) {
    sq::scripting::ComponentBinder<sq::ecs::Transform> binder(vm, POSITION_MODULE, POSITION_CLASS);
    binder.property<&sq::ecs::Transform::x>("x").property<&sq::ecs::Transform::y>("y");
    const auto result = vm.interpret(POSITION_MODULE, binder.generateWrenClass());
    if (!result) {
        sq::engine::log::error(LOG_TAG, "cannot bind ", POSITION_MODULE, " for the scripts: ", result.error());
        return false;
    }
    return true;
}

/** @brief Wren source of the `Combat` class, whose static methods are the foreign functions below. */
constexpr const char* COMBAT_SOURCE = R"(
class Combat {
    foreign static health(id)
    foreign static parts(id)
    foreign static partRole(id)
    foreign static nearestShip(x, y)
    foreign static fireAt(x, y, targetX, targetY, speed)
    foreign static fireAt(x, y, targetX, targetY, speed, look)
}
)";

/**
 * @brief Gives the registry of the room a foreign call runs in.
 * @param call Foreign call.
 * @return The registry, or null outside of a room VM.
 */
sq::ecs::Registry* registryOf(const sq::scripting::ForeignCall& call) {
    auto* scripts = call.userData<sq::scripting::ScriptSystem>();
    return scripts != nullptr ? scripts->getRegistry() : nullptr;
}

/**
 * @brief `Combat.health(id)`: `[points, max]`, or null for an entity without hit points.
 * @param call Foreign call.
 */
void combatHealth(sq::scripting::ForeignCall& call) {
    const auto id = call.arg<sq::ecs::Entity>(1);
    const sq::ecs::Registry* registry = registryOf(call);
    if (!id || registry == nullptr || !registry->isAlive(*id) || !registry->hasComponent<::prefabs::Health>(*id)) {
        call.retNull();
        return;
    }
    const ::prefabs::Health& health = registry->getComponents<::prefabs::Health>().get(*id);
    call.retList();
    call.listAppend(health.points);
    call.listAppend(health.max);
}

/**
 * @brief `Combat.parts(id)`: ids of the living parts of a boss.
 * @param call Foreign call.
 */
void combatParts(sq::scripting::ForeignCall& call) {
    const auto id = call.arg<sq::ecs::Entity>(1);
    const sq::ecs::Registry* registry = registryOf(call);
    if (!id) {
        return;
    }
    call.retList();
    if (registry == nullptr || !registry->isAlive(*id)) {
        return;
    }
    for (const sq::ecs::Entity child : sq::hierarchy::childrenOf(*registry, *id)) {
        if (registry->isAlive(child) && registry->hasComponent<::prefabs::BossPart>(child)) {
            call.listAppend(static_cast<double>(child));
        }
    }
}

/**
 * @brief `Combat.partRole(id)`: the role name of a boss part, or null.
 * @param call Foreign call.
 */
void combatPartRole(sq::scripting::ForeignCall& call) {
    const auto id = call.arg<sq::ecs::Entity>(1);
    const sq::ecs::Registry* registry = registryOf(call);
    if (!id || registry == nullptr || !registry->isAlive(*id) || !registry->hasComponent<::prefabs::BossPart>(*id)) {
        call.retNull();
        return;
    }
    switch (registry->getComponents<::prefabs::BossPart>().get(*id).role) {
        case ::prefabs::BossPartRole::WeakPoint:
            call.ret(::prefabs::BOSS_PART_WEAK_POINT);
            return;
        case ::prefabs::BossPartRole::Turret:
            call.ret(::prefabs::BOSS_PART_TURRET);
            return;
        case ::prefabs::BossPartRole::Armor:
            call.ret(::prefabs::BOSS_PART_ARMOR);
            return;
    }
}

/**
 * @brief `Combat.nearestShip(x, y)`: `[x, y]` of the nearest player ship that is not respawning, or null.
 * @param call Foreign call.
 */
void combatNearestShip(sq::scripting::ForeignCall& call) {
    const auto x = call.arg<double>(1);
    const auto y = call.arg<double>(2);
    const sq::ecs::Registry* registry = registryOf(call);
    if (!x || !y) {
        return;
    }
    const std::optional<AimPoint> ship =
        registry != nullptr ? nearestShip(*registry, static_cast<float>(*x), static_cast<float>(*y)) : std::nullopt;
    if (!ship) {
        call.retNull();
        return;
    }
    call.retList();
    call.listAppend(ship->x);
    call.listAppend(ship->y);
}

/**
 * @brief `Combat.fireAt(x, y, targetX, targetY, speed)`: spawns a missile flying towards the target.
 *
 * Goes through the prefab manager of the scripts, like `Engine.spawnPrefab`, so the missile is replicated and owned by
 * the script. A target on the shooter fires to the left.
 *
 * @param call Foreign call.
 */
void combatFireAt(sq::scripting::ForeignCall& call) {
    const auto x = call.arg<double>(1);
    const auto y = call.arg<double>(2);
    const auto targetX = call.arg<double>(3);
    const auto targetY = call.arg<double>(4);
    const auto speed = call.arg<double>(5);
    // `fireAt(x, y, targetX, targetY, speed, look)`: the look of the missile, its subtype, within a byte.
    const std::optional<double> look = call.slotCount() > 6 ? call.arg<double>(6) : std::optional<double>{0.0};
    auto* scripts = call.userData<sq::scripting::ScriptSystem>();
    auto* spawner = scripts != nullptr ? sq::prefabs::ScriptPrefabSpawner::of(*scripts) : nullptr;
    if (!x || !y || !targetX || !targetY || !speed || !look || spawner == nullptr) {
        return;
    }
    const auto clamped = static_cast<float>(std::clamp(*speed, MIN_AIMED_SHOT_SPEED, MAX_AIMED_SHOT_SPEED));
    const Velocity velocity =
        aimAt(static_cast<float>(*x), static_cast<float>(*y),
              AimPoint{.x = static_cast<float>(*targetX), .y = static_cast<float>(*targetY)}, clamped);
    const sq::prefabs::PrefabParams params{{"x", *x},
                                           {"y", *y},
                                           {"speed", velocity.x},
                                           {"speed_y", velocity.y},
                                           {"subtype", static_cast<std::int64_t>(std::clamp(*look, 0.0, 255.0))}};
    (void)spawner->spawn(::prefabs::MISSILE_PREFAB, params);
}

/**
 * @brief Registers the `Combat` foreign functions and interprets the class in one VM.
 * @param vm Wren VM.
 * @return True if the class was interpreted.
 */
bool bindCombat(sq::scripting::WrenVM& vm) {
    vm.registerForeignMethod<&combatHealth>(COMBAT_MODULE, "Combat", true, "health(_)");
    vm.registerForeignMethod<&combatParts>(COMBAT_MODULE, "Combat", true, "parts(_)");
    vm.registerForeignMethod<&combatPartRole>(COMBAT_MODULE, "Combat", true, "partRole(_)");
    vm.registerForeignMethod<&combatNearestShip>(COMBAT_MODULE, "Combat", true, "nearestShip(_,_)");
    vm.registerForeignMethod<&combatFireAt>(COMBAT_MODULE, "Combat", true, "fireAt(_,_,_,_,_)");
    vm.registerForeignMethod<&combatFireAt>(COMBAT_MODULE, "Combat", true, "fireAt(_,_,_,_,_,_)");
    const auto result = vm.interpret(COMBAT_MODULE, COMBAT_SOURCE);
    if (!result) {
        sq::engine::log::error(LOG_TAG, "cannot bind ", COMBAT_MODULE, " for the scripts: ", result.error());
        return false;
    }
    return true;
}

/**
 * @brief Binds every module of the boss scripts in one VM.
 * @param vm Wren VM.
 * @return True if every module was interpreted.
 */
bool bindAll(sq::scripting::WrenVM& vm) {
    const bool position = bindPosition(vm);
    const bool combat = bindCombat(vm);
    return position && combat;
}

}  // namespace

bool bindScriptComponents(sq::scripting::ScriptSystem& scripts) {
    scripts.addBinderReplay([](sq::scripting::WrenVM& vm) { (void)bindAll(vm); });
    return bindAll(scripts.getVm());
}

}  // namespace rtype::server::gameplay
