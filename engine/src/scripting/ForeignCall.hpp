/**
 * @file ForeignCall.hpp
 * @brief Type-safe and exception-safe wrapper for Wren foreign method invocations and slot marshaling.
 */

#pragma once
#include <optional>
#include <string_view>

#include "scripting/detail/WrenFwd.hpp"  // IWYU pragma: export

namespace scripting {

class WrenVM;

/**
 * @brief Helper context object wrapping raw Wren VM slots in foreign callbacks.
 *
 * Provides type validation, integer bound checking, zero-copy string views,
 * exception-safe fiber abortion, and return slot helpers.
 */
class ForeignCall {
  public:
    /**
     * @brief Constructs a ForeignCall over the VM a foreign method received.
     * @param vm The VM.
     */
    explicit ForeignCall(NativeVm* vm) noexcept : _vm(vm) {}

    /** @brief Returns the VM, opaque. */
    [[nodiscard]] NativeVm* raw() const noexcept { return _vm; }

    /** @brief Returns a pointer to the enclosing C++ WrenVM facade. */
    [[nodiscard]] WrenVM* vm() const noexcept;

    /**
     * @brief Retrieves the custom user context pointer registered with the WrenVM.
     * @tparam T Context type.
     * @return Pointer to user context or nullptr if missing.
     */
    template <typename T>
    [[nodiscard]] T* userData() const noexcept;

    /** @brief Returns the number of slots available in this foreign call. */
    [[nodiscard]] int slotCount() const noexcept { return detail::slots::count(_vm); }

    /**
     * @brief Returns the type of the value stored in a slot.
     * @param slot Slot index.
     */
    [[nodiscard]] SlotType slotType(int slot) const noexcept { return detail::slots::type(_vm, slot); }

    /**
     * @brief Aborts the current Wren fiber with an error message in slot 0.
     * @param message Descriptive error message.
     */
    void abort(std::string_view message) const noexcept {
        detail::slots::ensure(_vm, 1);
        detail::slots::setBytes(_vm, 0, message);
        detail::slots::abortFiber(_vm, 0);
    }

    /**
     * @brief Extracts and type-checks an argument from a slot.
     *
     * Validates that the slot exists and matches the requested type T. Rejects NaN and
     * infinite values for arithmetic types, and checks integer bounds. If validation
     * fails, automatically aborts the fiber with an error and returns std::nullopt.
     *
     * @tparam T Desired output type.
     * @param slot Zero-based slot index (slot 0 is receiver, slot 1+ are arguments).
     * @return Extracted value, or std::nullopt on type mismatch or bounds failure.
     */
    template <typename T>
    [[nodiscard]] std::optional<T> arg(int slot) const noexcept;

    /**
     * @brief Extracts a foreign object pointer stored in a slot.
     *
     * Validates that the slot contains a foreign object. Aborts fiber on mismatch.
     *
     * @tparam T Foreign object struct/class type.
     * @param slot Slot index.
     * @return Pointer to foreign object, or nullptr if slot is not foreign.
     */
    template <typename T>
    [[nodiscard]] T* argForeign(int slot) const noexcept {
        if (slot >= slotCount() || slotType(slot) != SlotType::Foreign) {
            abort("Expected argument to be a foreign object");
            return nullptr;
        }
        return static_cast<T*>(detail::slots::getForeign(_vm, slot));
    }

    /**
     * @brief Sets slot 0 to null.
     */
    void retNull() const noexcept { detail::slots::setNull(_vm, 0); }

    /**
     * @brief Allocates a new empty list in slot 0.
     */
    void retList() const noexcept {
        detail::slots::ensure(_vm, 1);
        detail::slots::setNewList(_vm, 0);
    }

    /**
     * @brief Appends a number to the list previously created in slot 0 by `retList()`.
     *
     * Uses slot 1 as scratch space, so call it only once every argument has been read.
     *
     * @param value Number to append.
     */
    void listAppend(double value) const noexcept {
        detail::slots::ensure(_vm, 2);
        detail::slots::setDouble(_vm, 1, value);
        detail::slots::insertInList(_vm, 0, -1, 1);
    }

    /**
     * @brief Writes a return value into slot 0.
     * @tparam T Value type.
     * @param value Value to return.
     */
    template <typename T>
    void ret(const T& value) const noexcept;

    /**
     * @brief Allocates a new foreign object in slot 0.
     * @tparam T Foreign data struct type.
     * @param classSlot Slot containing the foreign class object (default 0 for constructors).
     * @return Pointer to newly allocated foreign memory.
     */
    template <typename T>
    [[nodiscard]] T* retNewForeign(int classSlot = 0) const noexcept {
        return static_cast<T*>(detail::slots::setNewForeign(_vm, 0, classSlot, sizeof(T)));
    }

  private:
    /** @brief The VM. */
    NativeVm* _vm = nullptr;
};

}  // namespace scripting

#include "scripting/ForeignCall.tpp"  // IWYU pragma: export
