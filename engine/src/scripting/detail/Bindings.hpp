/**
 * @file Bindings.hpp
 * @brief Registration of the foreign methods of the core modules, on the live VM and on every new one.
 */

#pragma once

namespace sq::scripting {

class WrenVM;

}  // namespace sq::scripting

namespace sq::scripting::detail {

/**
 * @brief Registers the foreign methods of `core/Engine`, `core/Entity` and `core/World`.
 * @param vm VM whose user context is the `ScriptSystem`.
 */
void registerCoreBindings(WrenVM& vm);

}  // namespace sq::scripting::detail
