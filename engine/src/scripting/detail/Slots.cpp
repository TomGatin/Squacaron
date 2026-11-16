/**
 * @file Slots.cpp
 * @brief The slot primitives of `WrenFwd.hpp`, over the Wren C API.
 */

#include <cstddef>
#include <string_view>

#include "scripting/detail/WrenApi.hpp"
#include "scripting/detail/WrenFwd.hpp"

namespace scripting::detail::slots {

int count(NativeVm* vm) noexcept { return wrenGetSlotCount(vm); }

void ensure(NativeVm* vm, int slots) noexcept { wrenEnsureSlots(vm, slots); }

SlotType type(NativeVm* vm, int slot) noexcept {
    switch (wrenGetSlotType(vm, slot)) {
        case WREN_TYPE_BOOL:
            return SlotType::Bool;
        case WREN_TYPE_NUM:
            return SlotType::Num;
        case WREN_TYPE_FOREIGN:
            return SlotType::Foreign;
        case WREN_TYPE_LIST:
            return SlotType::List;
        case WREN_TYPE_MAP:
            return SlotType::Map;
        case WREN_TYPE_NULL:
            return SlotType::Null;
        case WREN_TYPE_STRING:
            return SlotType::String;
        default:
            return SlotType::Unknown;
    }
}

void setNull(NativeVm* vm, int slot) noexcept { wrenSetSlotNull(vm, slot); }

void setBool(NativeVm* vm, int slot, bool value) noexcept { wrenSetSlotBool(vm, slot, value); }

void setDouble(NativeVm* vm, int slot, double value) noexcept { wrenSetSlotDouble(vm, slot, value); }

void setBytes(NativeVm* vm, int slot, std::string_view value) noexcept {
    wrenSetSlotBytes(vm, slot, value.data(), value.size());
}

void setHandle(NativeVm* vm, int slot, ScriptObject* object) noexcept { wrenSetSlotHandle(vm, slot, object); }

void setNewList(NativeVm* vm, int slot) noexcept { wrenSetSlotNewList(vm, slot); }

void insertInList(NativeVm* vm, int list, int index, int element) noexcept {
    wrenInsertInList(vm, list, index, element);
}

void* setNewForeign(NativeVm* vm, int slot, int classSlot, std::size_t size) noexcept {
    return wrenSetSlotNewForeign(vm, slot, classSlot, size);
}

bool getBool(NativeVm* vm, int slot) noexcept { return wrenGetSlotBool(vm, slot); }

double getDouble(NativeVm* vm, int slot) noexcept { return wrenGetSlotDouble(vm, slot); }

std::string_view getBytes(NativeVm* vm, int slot) noexcept {
    int length = 0;
    const char* bytes = wrenGetSlotBytes(vm, slot, &length);
    return {bytes, static_cast<std::size_t>(length)};
}

ScriptObject* getHandle(NativeVm* vm, int slot) noexcept { return wrenGetSlotHandle(vm, slot); }

void* getForeign(NativeVm* vm, int slot) noexcept { return wrenGetSlotForeign(vm, slot); }

void abortFiber(NativeVm* vm, int slot) noexcept { wrenAbortFiber(vm, slot); }

void* userData(NativeVm* vm) noexcept { return wrenGetUserData(vm); }

void releaseHandle(NativeVm* vm, ScriptObject* object) noexcept { wrenReleaseHandle(vm, object); }

}  // namespace scripting::detail::slots
