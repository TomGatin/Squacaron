/**
 * @file WrenTypes.cpp
 * @brief Implementation of RAII deleter for Wren handles.
 */

#include "scripting/detail/WrenTypes.hpp"

#include "scripting/detail/WrenFwd.hpp"

namespace sq::scripting::detail {

void WrenHandleDeleter::operator()(ScriptObject* handle) const noexcept {
    if (handle != nullptr && vm != nullptr) {
        slots::releaseHandle(vm, handle);
    }
}

}  // namespace sq::scripting::detail
