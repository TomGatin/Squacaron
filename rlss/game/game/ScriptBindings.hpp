/**
 * @file ScriptBindings.hpp
 * @brief Exposes the components the RLSS scripts use to Wren.
 */

#pragma once

#include <string_view>

#include "scripting/ScriptSystem.hpp"
#include "scripting/WrenVM.hpp"

namespace rlss::game {

/** @brief Wren module holding the bound component classes: `import "rlss/components" for Transform, ...`. */
inline constexpr std::string_view COMPONENTS_MODULE = "rlss/components";

/**
 * @brief Defines the foreign classes `Transform` (x, y, rotation, scaleX, scaleY), `Controls` (playerIndex of the
 * entity's `sq::input::InputState`), `ArenaShape`, `CarParts`, `CarStatus`, `CarCues`, `BallCues`, `BallPart`,
 * `ShotGlow`, `MatchState`, `Camera` (the engine's `sq::ecs::CameraComponent`) and `CameraRig` in `COMPONENTS_MODULE`.
 * @param vm VM to define them in.
 * @return False if Wren refused the generated code.
 */
bool registerComponentClasses(sq::scripting::WrenVM& vm);

/**
 * @brief Registers the component classes in the script system's VM, and again each time hot-reload recreates it.
 * Call once, after `ScriptSystem::init`.
 * @param scripts Script system of the engine.
 * @return False if Wren refused the generated code.
 */
bool installScriptBindings(sq::scripting::ScriptSystem& scripts);

}  // namespace rlss::game
