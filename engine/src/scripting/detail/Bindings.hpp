/**
 * @file Bindings.hpp
 * @brief Registration of the foreign methods of the core modules, on the live VM and on every new one.
 */

#pragma once

namespace scripting {
class WrenVM;
}

namespace scripting::detail {

/**
 * @brief Registers the foreign methods of `core/Engine`, `core/Entity` and `core/World`.
 * @param vm VM whose user context is the `ScriptSystem`.
 */
void registerCoreBindings(WrenVM& vm);

}  // namespace scripting::detail
