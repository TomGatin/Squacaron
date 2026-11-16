/**
 * @file ForeignCall.tpp
 * @brief Template method implementations for ForeignCall argument and return marshaling.
 */

#pragma once
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include "scripting/ForeignCall.hpp"
#include "scripting/WrenVM.hpp"

namespace scripting {

inline WrenVM* ForeignCall::vm() const noexcept { return static_cast<WrenVM*>(detail::slots::userData(_vm)); }

template <typename T>
T* ForeignCall::userData() const noexcept {
    auto* wrapper = vm();
    if (wrapper == nullptr) {
        return nullptr;
    }
    return static_cast<T*>(wrapper->getUserContext());
}

template <typename T>
std::optional<T> ForeignCall::arg(int slot) const noexcept {
    using DecayedT = std::decay_t<T>;

    if (slot >= slotCount()) {
        abort("Slot index out of range");
        return std::nullopt;
    }

    if constexpr (std::is_same_v<DecayedT, bool>) {
        if (slotType(slot) != SlotType::Bool) {
            abort("Expected argument to be a boolean");
            return std::nullopt;
        }
        return detail::slots::getBool(_vm, slot);
    } else if constexpr (std::is_floating_point_v<DecayedT>) {
        if (slotType(slot) != SlotType::Num) {
            abort("Expected argument to be a number");
            return std::nullopt;
        }
        double const val = detail::slots::getDouble(_vm, slot);
        if (std::isnan(val) || std::isinf(val)) {
            abort("Number argument is NaN or infinite");
            return std::nullopt;
        }
        if (std::abs(val) > static_cast<double>(std::numeric_limits<DecayedT>::max())) {
            abort("Number argument out of floating-point bounds");
            return std::nullopt;
        }
        return static_cast<T>(val);
    } else if constexpr (std::is_integral_v<DecayedT>) {
        if (slotType(slot) != SlotType::Num) {
            abort("Expected argument to be a number");
            return std::nullopt;
        }
        double const val = detail::slots::getDouble(_vm, slot);
        if (std::isnan(val) || std::isinf(val)) {
            abort("Number argument is NaN or infinite");
            return std::nullopt;
        }
        if (std::trunc(val) != val) {
            abort("Expected argument to be an integer");
            return std::nullopt;
        }
        // The upper limit is exclusive: max() may round up to 2^digits once converted to double.
        if (val < static_cast<double>(std::numeric_limits<DecayedT>::lowest()) ||
            val >= std::ldexp(1.0, std::numeric_limits<DecayedT>::digits)) {
            abort("Number argument out of integer bounds");
            return std::nullopt;
        }
        return static_cast<T>(val);
    } else if constexpr (std::is_same_v<DecayedT, std::string_view>) {
        if (slotType(slot) != SlotType::String) {
            abort("Expected argument to be a string");
            return std::nullopt;
        }
        return detail::slots::getBytes(_vm, slot);
    } else if constexpr (std::is_same_v<DecayedT, std::string>) {
        if (slotType(slot) != SlotType::String) {
            abort("Expected argument to be a string");
            return std::nullopt;
        }
        return std::string(detail::slots::getBytes(_vm, slot));
    } else if constexpr (std::is_same_v<DecayedT, detail::UniqueWrenHandle>) {
        if (slotType(slot) == SlotType::Null) {
            abort("Expected non-null handle argument");
            return std::nullopt;
        }
        return detail::UniqueWrenHandle(detail::slots::getHandle(_vm, slot), detail::WrenHandleDeleter{.vm = _vm});
    } else if constexpr (std::is_same_v<DecayedT, ScriptObject*>) {
        return detail::slots::getHandle(_vm, slot);
    } else {
        static_assert(detail::always_false_v<DecayedT>, "Unsupported type for ForeignCall::arg");
        return std::nullopt;
    }
}

template <typename T>
void ForeignCall::ret(const T& value) const noexcept {
    using DecayedT = std::decay_t<T>;

    if constexpr (std::is_same_v<DecayedT, std::nullptr_t>) {
        (void)value;
        detail::slots::setNull(_vm, 0);
    } else if constexpr (std::is_same_v<DecayedT, bool>) {
        detail::slots::setBool(_vm, 0, value);
    } else if constexpr (std::is_arithmetic_v<DecayedT>) {
        detail::slots::setDouble(_vm, 0, static_cast<double>(value));
    } else if constexpr (std::is_convertible_v<DecayedT, std::string_view>) {
        detail::slots::setBytes(_vm, 0, std::string_view(value));
    } else if constexpr (std::is_same_v<DecayedT, ScriptObject*>) {
        detail::slots::setHandle(_vm, 0, value);
    } else if constexpr (std::is_same_v<DecayedT, detail::UniqueWrenHandle>) {
        detail::slots::setHandle(_vm, 0, value.get());
    } else {
        static_assert(detail::always_false_v<DecayedT>, "Unsupported type for ForeignCall::ret");
    }
}

}  // namespace scripting
