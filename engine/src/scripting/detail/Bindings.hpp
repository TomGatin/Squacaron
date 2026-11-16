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
 * @brief Registers the foreign methods of `core/Engine`, `core/Entity`, `core/World` and `core/Input`.
 * @param vm VM whose user context is the `ScriptSystem`.
 */
void registerCoreBindings(WrenVM& vm);

/**
 * @brief Registers the foreign methods of `core/Input`.
 * @param vm VM whose user context is the `ScriptSystem`.
 */
void registerInputBindings(WrenVM& vm);

}  // namespace scripting::detail
