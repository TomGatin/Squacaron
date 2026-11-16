/**
 * @file WrenVM.tpp
 * @brief Template method implementations for WrenVM slot marshaling and foreign trampolines.
 */

#pragma once
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#include "scripting/ForeignCall.hpp"
#include "scripting/WrenVM.hpp"

namespace sq::scripting {

template <auto Fn>
void WrenVM::foreignMethodTrampoline(NativeVm* rawVm) noexcept {
    try {
        ForeignCall call(rawVm);
        if constexpr (std::is_invocable_v<decltype(Fn), ForeignCall&>) {
            Fn(call);
        } else if constexpr (std::is_invocable_v<decltype(Fn), NativeVm*>) {
            Fn(rawVm);
        } else {
            static_assert(detail::always_false_v<decltype(Fn)>, "Foreign method must accept ForeignCall& or NativeVm*");
        }
    } catch (const std::exception& e) {
        ForeignCall call(rawVm);
        call.abort(std::string("C++ exception: ") + e.what());
    } catch (...) {
        ForeignCall call(rawVm);
        call.abort("Unknown C++ exception");
    }
}

template <auto Fn>
void WrenVM::registerForeignMethod(std::string_view module, std::string_view className, bool isStatic,
                                   std::string_view signature) {
    registerForeignMethod(module, className, isStatic, signature, &foreignMethodTrampoline<Fn>);
}

template <typename Receiver, typename... Args>
CallResult WrenVM::call(ScriptObject* method, const Receiver& receiver, const Args&... args) {
    constexpr int totalSlots = 1 + sizeof...(Args);
    ensureSlots(totalSlots);
    setSlot(0, receiver);
    int slot = 1;
    ([&]() { setSlot(slot++, args); }(), ...);
    return call(method);
}

template <typename Receiver, typename... Args>
CallResult WrenVM::call(const detail::UniqueWrenHandle& method, const Receiver& receiver, const Args&... args) {
    return call(method.get(), receiver, args...);
}

template <typename... Args>
detail::UniqueWrenHandle WrenVM::newInstance(std::string_view module, std::string_view className, const Args&... args) {
    if (!hasVariable(module, className)) {
        return detail::UniqueWrenHandle(nullptr, detail::WrenHandleDeleter{.vm = _vm});
    }
    constexpr int totalSlots = 1 + sizeof...(Args);
    ensureSlots(totalSlots);
    getVariable(module, className, 0);
    int slot = 1;
    ([&]() { setSlot(slot++, args); }(), ...);

    auto newHandle = makeCallHandle(detail::makeMethodSignature("new", sizeof...(Args)));
    if (call(newHandle.get()) != CallResult::Success) {
        return detail::UniqueWrenHandle(nullptr, detail::WrenHandleDeleter{.vm = _vm});
    }
    return getSlot<detail::UniqueWrenHandle>(0);
}

template <typename... Args>
detail::UniqueWrenHandle WrenVM::newInstanceWithHandle(const detail::UniqueWrenHandle& newHandle,
                                                       std::string_view module, std::string_view className,
                                                       const Args&... args) {
    if (!hasVariable(module, className)) {
        return detail::UniqueWrenHandle(nullptr, detail::WrenHandleDeleter{.vm = _vm});
    }
    constexpr int totalSlots = 1 + sizeof...(Args);
    ensureSlots(totalSlots);
    getVariable(module, className, 0);
    int slot = 1;
    ([&]() { setSlot(slot++, args); }(), ...);

    if (call(newHandle.get()) != CallResult::Success) {
        return detail::UniqueWrenHandle(nullptr, detail::WrenHandleDeleter{.vm = _vm});
    }
    return getSlot<detail::UniqueWrenHandle>(0);
}

template <typename T>
void WrenVM::setSlot(int slot, const T& value) {
    using DecayedT = std::decay_t<T>;
    if constexpr (std::is_same_v<DecayedT, std::nullptr_t>) {
        (void)value;
        detail::slots::setNull(_vm, slot);
    } else if constexpr (std::is_same_v<DecayedT, bool>) {
        detail::slots::setBool(_vm, slot, value);
    } else if constexpr (std::is_arithmetic_v<DecayedT>) {
        detail::slots::setDouble(_vm, slot, static_cast<double>(value));
    } else if constexpr (std::is_convertible_v<DecayedT, std::string_view>) {
        detail::slots::setBytes(_vm, slot, std::string_view(value));
    } else if constexpr (std::is_same_v<DecayedT, ScriptObject*>) {
        detail::slots::setHandle(_vm, slot, value);
    } else if constexpr (std::is_same_v<DecayedT, detail::UniqueWrenHandle>) {
        detail::slots::setHandle(_vm, slot, value.get());
    } else {
        static_assert(detail::always_false_v<DecayedT>, "Unsupported slot type for WrenVM::setSlot");
    }
}

template <typename T>
std::optional<T> WrenVM::getSlotChecked(int slot) {
    using DecayedT = std::decay_t<T>;

    if (slot < 0 || slot >= detail::slots::count(_vm)) {
        return std::nullopt;
    }

    if constexpr (std::is_same_v<DecayedT, bool>) {
        if (detail::slots::type(_vm, slot) != SlotType::Bool) {
            return std::nullopt;
        }
        return detail::slots::getBool(_vm, slot);
    } else if constexpr (std::is_floating_point_v<DecayedT>) {
        if (detail::slots::type(_vm, slot) != SlotType::Num) {
            return std::nullopt;
        }
        double const d = detail::slots::getDouble(_vm, slot);
        if (std::isnan(d) || std::isinf(d)) {
            return std::nullopt;
        }
        return static_cast<T>(d);
    } else if constexpr (std::is_integral_v<DecayedT>) {
        if (detail::slots::type(_vm, slot) != SlotType::Num) {
            return std::nullopt;
        }
        double const d = detail::slots::getDouble(_vm, slot);
        if (std::isnan(d) || std::isinf(d)) {
            return std::nullopt;
        }
        // The upper limit is exclusive: max() may round up to 2^digits once converted to double (e.g. for 64-bit
        // types such as sq::ecs::Entity), and converting 2^digits back to the integer type is undefined behaviour.
        if (d < static_cast<double>(std::numeric_limits<DecayedT>::lowest()) ||
            d >= std::ldexp(1.0, std::numeric_limits<DecayedT>::digits)) {
            return std::nullopt;
        }
        return static_cast<T>(d);
    } else if constexpr (std::is_same_v<DecayedT, std::string_view>) {
        if (detail::slots::type(_vm, slot) != SlotType::String) {
            return std::nullopt;
        }
        return detail::slots::getBytes(_vm, slot);
    } else if constexpr (std::is_same_v<DecayedT, std::string>) {
        if (detail::slots::type(_vm, slot) != SlotType::String) {
            return std::nullopt;
        }
        return std::string(detail::slots::getBytes(_vm, slot));
    } else if constexpr (std::is_same_v<DecayedT, ScriptObject*>) {
        return detail::slots::getHandle(_vm, slot);
    } else if constexpr (std::is_same_v<DecayedT, detail::UniqueWrenHandle>) {
        return detail::UniqueWrenHandle(detail::slots::getHandle(_vm, slot), detail::WrenHandleDeleter{.vm = _vm});
    } else {
        static_assert(detail::always_false_v<DecayedT>, "Unsupported slot type for WrenVM::getSlotChecked");
        return std::nullopt;
    }
}

template <typename T>
T WrenVM::getSlot(int slot) {
    auto opt = getSlotChecked<T>(slot);
    if (!opt.has_value()) {
        throw std::runtime_error("Wren slot type mismatch or out of bounds for requested type");
    }
    return std::move(*opt);
}

}  // namespace sq::scripting
