/**
 * @file WrenFwd.hpp
 * @brief Engine names for the Wren C types, and the slot primitives the templates are built on. No `wren.h`.
 *
 * The Wren structs are only declared here: a game including the engine headers never needs `wren.h`. Only the
 * scripting module's `.cpp` files include it, through `scripting/detail/WrenApi.hpp`.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

// Declared, never defined outside wren: `NativeVm` and `ScriptObject` are only used through pointers.
struct WrenVM;      // NOLINT(readability-identifier-naming): the name is Wren's
struct WrenHandle;  // NOLINT(readability-identifier-naming): the name is Wren's

namespace sq::scripting {

/** @brief The Wren C virtual machine, opaque. A foreign method receives it. */
using NativeVm = ::WrenVM;

/** @brief A Wren object kept alive by the VM (an instance, a fiber, a call handle), opaque. */
using ScriptObject = ::WrenHandle;

/** @brief A foreign method as Wren calls it. Use `WrenVM::registerForeignMethod<&fn>` to get one from a function. */
using ForeignMethodFn = void (*)(NativeVm* vm);

/** @brief Releases the C++ data of a foreign object when Wren collects it. */
using ForeignFinalizerFn = void (*)(void* data);

/** @brief Allocation and finalization of a foreign class. */
struct ForeignClassMethods {
    /** @brief Called by the constructor: allocates the object with `ForeignCall::retNewForeign`. */
    ForeignMethodFn allocate = nullptr;
    /** @brief Called when the object is collected; may be null. */
    ForeignFinalizerFn finalize = nullptr;
};

/** @brief Outcome of running Wren code. */
enum class CallResult : std::uint8_t {
    /** @brief The code ran to its end. */
    Success,
    /** @brief The source did not compile. */
    CompileError,
    /** @brief The code aborted (runtime error, `Fiber.abort`). */
    RuntimeError,
};

/** @brief Kind of an error reported to `WrenVM::ErrorHandler`. */
enum class ScriptErrorKind : std::uint8_t {
    /** @brief A compile error, with its module and line. */
    Compile,
    /** @brief The message of a runtime error. */
    Runtime,
    /** @brief One line of the stack trace of a runtime error. */
    StackTrace,
};

/** @brief Type of the value in a Wren slot. */
enum class SlotType : std::uint8_t {
    /** @brief A boolean. */
    Bool,
    /** @brief A number. */
    Num,
    /** @brief A foreign object. */
    Foreign,
    /** @brief A list. */
    List,
    /** @brief A map. */
    Map,
    /** @brief Null. */
    Null,
    /** @brief A string. */
    String,
    /** @brief Any other object. */
    Unknown,
};

/** @brief Raw slot access, implemented over the Wren C API in `Slots.cpp`. Used by the engine templates. */

}  // namespace sq::scripting

namespace sq::scripting::detail::slots {

/** @brief Number of slots. @param vm VM. @return The count. */
[[nodiscard]] int count(NativeVm* vm) noexcept;
/** @brief Makes at least `slots` slots available. @param vm VM. @param slots Count. */
void ensure(NativeVm* vm, int slots) noexcept;
/** @brief Type of a slot. @param vm VM. @param slot Slot. @return Its type. */
[[nodiscard]] SlotType type(NativeVm* vm, int slot) noexcept;
/** @brief Stores null. @param vm VM. @param slot Slot. */
void setNull(NativeVm* vm, int slot) noexcept;
/** @brief Stores a boolean. @param vm VM. @param slot Slot. @param value Value. */
void setBool(NativeVm* vm, int slot, bool value) noexcept;
/** @brief Stores a number. @param vm VM. @param slot Slot. @param value Value. */
void setDouble(NativeVm* vm, int slot, double value) noexcept;
/** @brief Stores a string (copied). @param vm VM. @param slot Slot. @param value Bytes. */
void setBytes(NativeVm* vm, int slot, std::string_view value) noexcept;
/** @brief Stores the object of a handle. @param vm VM. @param slot Slot. @param object Handle. */
void setHandle(NativeVm* vm, int slot, ScriptObject* object) noexcept;
/** @brief Stores a new empty list. @param vm VM. @param slot Slot. */
void setNewList(NativeVm* vm, int slot) noexcept;
/** @brief Inserts an element in a list. @param vm VM. @param list List slot. @param index Index (-1 appends).
 * @param element Element slot. */
void insertInList(NativeVm* vm, int list, int index, int element) noexcept;
/** @brief Allocates a foreign object. @param vm VM. @param slot Slot. @param classSlot Slot of its class.
 * @param size Bytes. @return Its memory. */
[[nodiscard]] void* setNewForeign(NativeVm* vm, int slot, int classSlot, std::size_t size) noexcept;
/** @brief Reads a boolean. @param vm VM. @param slot Slot. @return Value. */
[[nodiscard]] bool getBool(NativeVm* vm, int slot) noexcept;
/** @brief Reads a number. @param vm VM. @param slot Slot. @return Value. */
[[nodiscard]] double getDouble(NativeVm* vm, int slot) noexcept;
/** @brief Reads a string; valid until the slot changes. @param vm VM. @param slot Slot. @return Bytes. */
[[nodiscard]] std::string_view getBytes(NativeVm* vm, int slot) noexcept;
/** @brief Makes a handle to the object of a slot. @param vm VM. @param slot Slot. @return New handle. */
[[nodiscard]] ScriptObject* getHandle(NativeVm* vm, int slot) noexcept;
/** @brief Reads the memory of a foreign object. @param vm VM. @param slot Slot. @return Its memory. */
[[nodiscard]] void* getForeign(NativeVm* vm, int slot) noexcept;
/** @brief Aborts the running fiber with the value of a slot. @param vm VM. @param slot Slot. */
void abortFiber(NativeVm* vm, int slot) noexcept;
/** @brief User data of the VM: the engine `WrenVM`. @param vm VM. @return The pointer. */
[[nodiscard]] void* userData(NativeVm* vm) noexcept;
/** @brief Releases a handle. @param vm VM. @param object Handle. */
void releaseHandle(NativeVm* vm, ScriptObject* object) noexcept;

}  // namespace sq::scripting::detail::slots
