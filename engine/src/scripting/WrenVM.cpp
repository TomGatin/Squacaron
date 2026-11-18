/**
 * @file WrenVM.cpp
 * @brief Implementation of the WrenVM high-level wrapper and FFI dispatchers.
 */

#include "scripting/WrenVM.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <utility>

#include "scripting/detail/WrenApi.hpp"
#include "scripting/detail/WrenFwd.hpp"
#include "scripting/detail/WrenTypes.hpp"

namespace sq::scripting::detail {

std::string makeMethodSignature(std::string_view name, std::size_t arity) {
    std::string sig(name);
    sig += "(";
    for (std::size_t i = 0; i < arity; ++i) {
        if (i > 0) {
            sig += ",";
        }
        sig += "_";
    }
    sig += ")";
    return sig;
}

}  // namespace sq::scripting::detail

namespace sq::scripting {

WrenVM::WrenVM() { createVm(); }

NativeVm* detail::raw(WrenVM& vm) noexcept { return vm._vm; }

WrenVM::~WrenVM() {
    if (_vm != nullptr) {
        wrenFreeVM(_vm);
        _vm = nullptr;
    }
}

WrenVM::WrenVM(WrenVM&& other) noexcept
    : _vm(other._vm),
      _registry(other._registry),
      _userContext(other._userContext),
      _moduleLoader(std::move(other._moduleLoader)),
      _writeHandler(std::move(other._writeHandler)),
      _errorHandler(std::move(other._errorHandler)),
      _lastError(std::move(other._lastError)),
      _pendingOutput(std::move(other._pendingOutput)),
      _loadedSources(std::move(other._loadedSources)),
      _foreignMethods(std::move(other._foreignMethods)),
      _foreignClasses(std::move(other._foreignClasses)) {
    other._vm = nullptr;
    other._registry = nullptr;
    other._userContext = nullptr;
    if (_vm != nullptr) {
        wrenSetUserData(_vm, this);
    }
}

WrenVM& WrenVM::operator=(WrenVM&& other) noexcept {
    if (this != &other) {
        if (_vm != nullptr) {
            wrenFreeVM(_vm);
        }
        _vm = other._vm;
        _registry = other._registry;
        _userContext = other._userContext;
        _moduleLoader = std::move(other._moduleLoader);
        _writeHandler = std::move(other._writeHandler);
        _errorHandler = std::move(other._errorHandler);
        _lastError = std::move(other._lastError);
        _pendingOutput = std::move(other._pendingOutput);
        _loadedSources = std::move(other._loadedSources);
        _foreignMethods = std::move(other._foreignMethods);
        _foreignClasses = std::move(other._foreignClasses);
        other._vm = nullptr;
        other._registry = nullptr;
        other._userContext = nullptr;
        if (_vm != nullptr) {
            wrenSetUserData(_vm, this);
        }
    }
    return *this;
}

void WrenVM::setModuleLoader(ModuleLoader loader) { _moduleLoader = std::move(loader); }

const WrenVM::ModuleLoader& WrenVM::getModuleLoader() const noexcept { return _moduleLoader; }

void WrenVM::reset() {
    if (_vm != nullptr) {
        wrenFreeVM(_vm);
        _vm = nullptr;
    }
    _loadedSources.clear();
    _lastError.clear();
    _pendingOutput.clear();
    createVm();
}

void WrenVM::setWriteHandler(WriteHandler handler) { _writeHandler = std::move(handler); }

void WrenVM::setErrorHandler(ErrorHandler handler) { _errorHandler = std::move(handler); }

void WrenVM::registerForeignMethod(std::string_view module, std::string_view className, bool isStatic,
                                   std::string_view signature, ForeignMethodFn fn) {
    _foreignMethods[detail::ForeignMethodKey{.module = std::string(module),
                                             .className = std::string(className),
                                             .isStatic = isStatic,
                                             .signature = std::string(signature)}] = fn;
}

void WrenVM::registerForeignClass(std::string_view module, std::string_view className, ForeignClassMethods methods) {
    _foreignClasses[std::string(module) + "/" + std::string(className)] = methods;
}

std::expected<void, std::string> WrenVM::interpret(std::string_view module, std::string_view source) {
    _lastError.clear();
    std::string const modStr(module);
    std::string const srcStr(source);
    if (wrenInterpret(_vm, modStr.c_str(), srcStr.c_str()) != WREN_RESULT_SUCCESS) {
        return std::unexpected(_lastError.empty() ? "Wren interpretation error" : _lastError);
    }
    return {};
}

detail::UniqueWrenHandle WrenVM::makeCallHandle(std::string_view signature) {
    std::string const sigStr(signature);
    ScriptObject* handle = wrenMakeCallHandle(_vm, sigStr.c_str());
    return detail::UniqueWrenHandle(handle, detail::WrenHandleDeleter{.vm = _vm});
}

void WrenVM::releaseHandle(ScriptObject* handle) {
    if (handle != nullptr && _vm != nullptr) {
        wrenReleaseHandle(_vm, handle);
    }
}

CallResult WrenVM::call(ScriptObject* method) {
    _lastError.clear();
    switch (wrenCall(_vm, method)) {
        case WREN_RESULT_SUCCESS:
            return CallResult::Success;
        case WREN_RESULT_COMPILE_ERROR:
            return CallResult::CompileError;
        default:
            return CallResult::RuntimeError;
    }
}

CallResult WrenVM::call(const detail::UniqueWrenHandle& method) { return call(method.get()); }

bool WrenVM::hasModule(std::string_view module) {
    if (_vm == nullptr) {
        return false;
    }
    std::string const modStr(module);
    return wrenHasModule(_vm, modStr.c_str());
}

bool WrenVM::hasVariable(std::string_view module, std::string_view variableName) {
    if (_vm == nullptr || !hasModule(module)) {
        return false;
    }
    std::string const modStr(module);
    std::string const varStr(variableName);
    return wrenHasVariable(_vm, modStr.c_str(), varStr.c_str());
}

void WrenVM::getVariable(std::string_view module, std::string_view variableName, int slot) {
    if (_vm == nullptr || !hasModule(module)) {
        return;
    }
    std::string const modStr(module);
    std::string const varStr(variableName);
    wrenGetVariable(_vm, modStr.c_str(), varStr.c_str(), slot);
}

void WrenVM::collectGarbage() {
    if (_vm != nullptr) {
        wrenCollectGarbage(_vm);
    }
}

void WrenVM::ensureSlots(int numSlots) { wrenEnsureSlots(_vm, numSlots); }

}  // namespace sq::scripting
