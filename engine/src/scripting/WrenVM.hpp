/**
 * @file WrenVM.hpp
 * @brief RAII wrapper of the Wren virtual machine: modules, foreign bindings, calls and slots. No `wren.h`.
 */

#pragma once
#include <cstddef>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "scripting/detail/WrenFwd.hpp"    // IWYU pragma: export
#include "scripting/detail/WrenTypes.hpp"  // IWYU pragma: export

namespace sq::scripting {

class ForeignCall;
class WrenVM;

}  // namespace sq::scripting

namespace sq::scripting::detail {

/**
 * @brief Builds a Wren method signature.
 * @param name Method name.
 * @param arity Number of parameters.
 * @return The signature, e.g. `new(_,_)`.
 */
[[nodiscard]] std::string makeMethodSignature(std::string_view name, std::size_t arity);

/**
 * @brief Gets the Wren C VM of a wrapper, for code that includes `scripting/detail/WrenApi.hpp` (the scripting
 * module, tests). Games use the `WrenVM` and `ForeignCall` API instead.
 * @param vm Wrapper.
 * @return The C VM.
 */
[[nodiscard]] NativeVm* raw(WrenVM& vm) noexcept;

/** @brief The callbacks the Wren C VM calls into, defined in `WrenVMCallbacks.cpp`. */
struct VmCallbacks;

}  // namespace sq::scripting::detail

namespace sq::scripting {

/**
 * @brief Owns a Wren virtual machine.
 *
 * - Modules: `interpret`, `hasModule`, `hasVariable`, and a `ModuleLoader` for `import`.
 * - Foreign methods and classes: registered by name before the code that declares them runs; they survive `reset()`.
 * - Calls: `makeCallHandle` then `call(method, receiver, args...)`, or `newInstance`; owned handles are
 *   `detail::UniqueWrenHandle`.
 * - Slots: `setSlot<T>` / `getSlot<T>` / `getSlotChecked<T>` for bool, numbers, strings and handles.
 *
 * Its header only declares the Wren C types: the scripting module is the only code that includes `wren.h`.
 */
class WrenVM {
  public:
    /** @brief Gives the source of an imported module, or an empty string if it does not exist. */
    using ModuleLoader = std::function<std::string(std::string_view moduleName)>;
    /** @brief Receives the text printed by `System.print`. */
    using WriteHandler = std::function<void(std::string_view text)>;
    /** @brief Receives each piece of an error: compile error, runtime error message, stack trace line. */
    using ErrorHandler =
        std::function<void(ScriptErrorKind kind, std::string_view module, int line, std::string_view message)>;

    /** @brief Creates a VM. */
    WrenVM();
    /** @brief Frees the VM; its handles must be released first. */
    ~WrenVM();
    /** @brief Deleted copy constructor. */
    WrenVM(const WrenVM&) = delete;
    /** @brief Deleted copy assignment operator. */
    WrenVM& operator=(const WrenVM&) = delete;
    /** @brief Takes over the VM, its bindings and callbacks. @param other Source, left empty. */
    WrenVM(WrenVM&& other) noexcept;
    /** @brief Frees this VM and takes over `other`. @param other Source, left empty. @return This. */
    WrenVM& operator=(WrenVM&& other) noexcept;

    /** @brief Sets the loader called on `import`. @param loader The loader. */
    void setModuleLoader(ModuleLoader loader);
    /** @brief Gets the loader called on `import`. @return The loader. */
    [[nodiscard]] const ModuleLoader& getModuleLoader() const noexcept;
    /** @brief Replaces the VM by a new one, keeping the foreign bindings and the callbacks. */
    void reset();
    /** @brief Sets the handler of `System.print`. @param handler The handler; empty logs the text. */
    void setWriteHandler(WriteHandler handler);
    /** @brief Sets the handler of the errors. @param handler The handler; empty logs them in verbose mode. */
    void setErrorHandler(ErrorHandler handler);
    /** @brief Gets the handler of `System.print`, to restore it later. @return The handler. */
    [[nodiscard]] const WriteHandler& writeHandler() const noexcept { return _writeHandler; }
    /** @brief Gets the handler of the errors, to restore it later. @return The handler. */
    [[nodiscard]] const ErrorHandler& errorHandler() const noexcept { return _errorHandler; }

    /**
     * @brief Registers a foreign method from a function Wren can call directly.
     * @param module Module of the class.
     * @param className Class.
     * @param isStatic True for a static method.
     * @param signature Wren signature (e.g. `wait(_)` or `x=(_)`).
     * @param fn The function.
     */
    void registerForeignMethod(std::string_view module, std::string_view className, bool isStatic,
                               std::string_view signature, ForeignMethodFn fn);

    /**
     * @brief Registers a foreign method calling `Fn` through a noexcept trampoline: a C++ exception aborts the
     * fiber with its message instead of crossing the C VM.
     * @tparam Fn Function taking `ForeignCall&` (or `NativeVm*`).
     * @param module Module of the class.
     * @param className Class.
     * @param isStatic True for a static method.
     * @param signature Wren signature.
     */
    template <auto Fn>
    void registerForeignMethod(std::string_view module, std::string_view className, bool isStatic,
                               std::string_view signature);

    /**
     * @brief Registers the allocation and finalization of a foreign class.
     * @param module Module of the class.
     * @param className Class.
     * @param methods The functions.
     */
    void registerForeignClass(std::string_view module, std::string_view className, ForeignClassMethods methods);

    /**
     * @brief Runs Wren source in a module.
     * @param module Module name.
     * @param source Source.
     * @return Nothing, or the error (`lastError()`).
     */
    std::expected<void, std::string> interpret(std::string_view module, std::string_view source);

    /** @brief Makes a handle to call methods of a signature. @param signature E.g. `update(_)`. @return It. */
    [[nodiscard]] detail::UniqueWrenHandle makeCallHandle(std::string_view signature);
    /** @brief Releases a handle made by this VM. @param handle Handle; null is ignored. */
    void releaseHandle(ScriptObject* handle);
    /** @brief Calls a method; receiver and arguments are already in the slots. @param method Call handle. */
    CallResult call(ScriptObject* method);
    /** @brief Calls a method; receiver and arguments are already in the slots. @param method Call handle. */
    CallResult call(const detail::UniqueWrenHandle& method);

    /**
     * @brief Calls a method on a receiver with arguments, written to the slots first.
     * @param method Call handle.
     * @param receiver Receiver (a handle, or a value for static calls on a class in slot 0).
     * @param args Arguments.
     * @return The result.
     */
    template <typename Receiver, typename... Args>
    CallResult call(ScriptObject* method, const Receiver& receiver, const Args&... args);
    /** @copydoc call(ScriptObject*, const Receiver&, const Args&...) */
    template <typename Receiver, typename... Args>
    CallResult call(const detail::UniqueWrenHandle& method, const Receiver& receiver, const Args&... args);

    /** @brief Error of the last failed `interpret()` or `call()`: message, then the stack trace, one per line. */
    [[nodiscard]] const std::string& lastError() const noexcept { return _lastError; }
    /** @brief Longest text kept by `lastError()`, in bytes: a deep stack trace is cut there. */
    static constexpr std::size_t MAX_ERROR_LENGTH = 4096;

    /**
     * @brief Instantiates a class with `new(args...)`.
     * @param module Module of the class.
     * @param className Class.
     * @param args Constructor arguments.
     * @return Handle to the instance, or a null handle on failure.
     */
    template <typename... Args>
    [[nodiscard]] detail::UniqueWrenHandle newInstance(std::string_view module, std::string_view className,
                                                       const Args&... args);
    /**
     * @brief Instantiates a class with a cached constructor handle.
     * @param newHandle Handle of the constructor signature.
     * @param module Module of the class.
     * @param className Class.
     * @param args Constructor arguments.
     * @return Handle to the instance, or a null handle on failure.
     */
    template <typename... Args>
    [[nodiscard]] detail::UniqueWrenHandle newInstanceWithHandle(const detail::UniqueWrenHandle& newHandle,
                                                                 std::string_view module, std::string_view className,
                                                                 const Args&... args);

    /** @brief Tells whether a module is loaded. @param module Module. @return True if loaded. */
    [[nodiscard]] bool hasModule(std::string_view module);
    /** @brief Tells whether a module defines a variable. @param module Module. @param variableName Name. */
    bool hasVariable(std::string_view module, std::string_view variableName);
    /** @brief Loads a module variable into a slot. @param module Module. @param variableName Name. @param slot Slot. */
    void getVariable(std::string_view module, std::string_view variableName, int slot);
    /** @brief Runs a garbage collection. */
    void collectGarbage();

    /** @brief Sets the ECS registry the component bindings read. @param registry `sq::ecs::Registry*`. */
    void setRegistry(void* registry) noexcept { _registry = registry; }
    /** @brief Gets the ECS registry of the component bindings. @return `sq::ecs::Registry*`, or null. */
    [[nodiscard]] void* getRegistry() const noexcept { return _registry; }
    /** @brief Sets the object foreign methods reach with `ForeignCall::userData`. @param ctx Context. */
    void setUserContext(void* ctx) noexcept { _userContext = ctx; }
    /** @brief Gets the object given to `setUserContext`. @return The context, or null. */
    [[nodiscard]] void* getUserContext() const noexcept { return _userContext; }

    /** @brief Makes at least `numSlots` slots available. @param numSlots Count. */
    void ensureSlots(int numSlots);
    /** @brief Writes a value (null, bool, number, string, handle) to a slot. @param slot Slot. @param value Value. */
    template <typename T>
    void setSlot(int slot, const T& value);
    /**
     * @brief Reads a slot as `T` (bool, number with range check, string, handle).
     * @param slot Slot.
     * @return The value, or nothing if the slot does not exist or holds another type.
     */
    template <typename T>
    [[nodiscard]] std::optional<T> getSlotChecked(int slot);
    /** @brief Reads a slot as `T`. @param slot Slot. @return The value. @throws std::runtime_error On mismatch. */
    template <typename T>
    [[nodiscard]] T getSlot(int slot);

  private:
    friend struct detail::VmCallbacks;
    friend NativeVm* detail::raw(WrenVM& vm) noexcept;

    /** @brief Calls `Fn` for Wren, turning C++ exceptions into fiber aborts. @param rawVm The VM. */
    template <auto Fn>
    static void foreignMethodTrampoline(NativeVm* rawVm) noexcept;
    /** @brief Creates the C VM with the callbacks. */
    void createVm();

    /** @brief The C VM. */
    NativeVm* _vm = nullptr;
    /** @brief `sq::ecs::Registry*` of the component bindings. */
    void* _registry = nullptr;
    /** @brief Context of the foreign methods. */
    void* _userContext = nullptr;
    /** @brief Loader called on `import`. */
    ModuleLoader _moduleLoader;
    /** @brief Handler of `System.print`. */
    WriteHandler _writeHandler;
    /** @brief Handler of the errors. */
    ErrorHandler _errorHandler;
    /** @brief Error of the last failed `interpret()` or `call()`. */
    std::string _lastError;
    /** @brief Printed text without its newline yet, logged once the line is complete. */
    std::string _pendingOutput;
    /** @brief Sources of the imported modules, kept until the VM copied them. */
    std::unordered_map<std::string, std::string> _loadedSources;
    /** @brief Foreign methods, by module, class, staticness and signature. */
    std::unordered_map<detail::ForeignMethodKey, ForeignMethodFn, detail::ForeignMethodKeyHash> _foreignMethods;
    /** @brief Foreign classes, by "module/class". */
    std::unordered_map<std::string, ForeignClassMethods> _foreignClasses;
};

}  // namespace sq::scripting

#include "WrenVM.tpp"  // IWYU pragma: export
