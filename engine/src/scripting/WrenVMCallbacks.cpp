/**
 * @file WrenVMCallbacks.cpp
 * @brief Callbacks the Wren C VM calls into: foreign bindings, module imports, `System.print` and errors.
 */

#include <cstddef>
#include <exception>
#include <string>
#include <string_view>
#include <utility>

#include "log/Logger.hpp"
#include "scripting/WrenVM.hpp"
#include "scripting/detail/LogTag.hpp"
#include "scripting/detail/WrenApi.hpp"
#include "scripting/detail/WrenTypes.hpp"

namespace sq::scripting::detail {

namespace {

/**
 * @brief Appends one error line to `error`, unless it would go past `WrenVM::MAX_ERROR_LENGTH`.
 * @param error Error text being built.
 * @param line Line to add, without its newline.
 */
void appendErrorLine(std::string& error, std::string_view line) {
    constexpr std::string_view CUT_MARK = "...";
    if (error.ends_with(CUT_MARK)) {
        return;
    }
    if (error.size() + line.size() + 1 > WrenVM::MAX_ERROR_LENGTH) {
        error += CUT_MARK;
        return;
    }
    if (!error.empty()) {
        error += '\n';
    }
    error += line;
}

}  // namespace

/** @brief The callbacks given to the Wren C VM; a friend of `WrenVM`. */
struct VmCallbacks {
    /** @brief Finds a registered foreign method. */
    static WrenForeignMethodFn onBindForeignMethod(::WrenVM* vm, const char* module, const char* className,
                                                   bool isStatic, const char* signature) noexcept;
    /** @brief Finds a registered foreign class. */
    static WrenForeignClassMethods onBindForeignClass(::WrenVM* vm, const char* module, const char* className) noexcept;
    /** @brief Resolves an `import` with the module loader. */
    static WrenLoadModuleResult onLoadModule(::WrenVM* vm, const char* name) noexcept;
    /** @brief Receives `System.print` output. */
    static void onWrite(::WrenVM* vm, const char* text) noexcept;
    /** @brief Receives compile errors, runtime errors and stack trace lines. */
    static void onError(::WrenVM* vm, WrenErrorType type, const char* module, int line, const char* message) noexcept;
};

}  // namespace sq::scripting::detail

namespace sq::scripting {

void WrenVM::createVm() {
    WrenConfiguration config;
    wrenInitConfiguration(&config);
    config.userData = this;
    config.bindForeignMethodFn = &detail::VmCallbacks::onBindForeignMethod;
    config.bindForeignClassFn = &detail::VmCallbacks::onBindForeignClass;
    config.loadModuleFn = &detail::VmCallbacks::onLoadModule;
    config.writeFn = &detail::VmCallbacks::onWrite;
    config.errorFn = &detail::VmCallbacks::onError;
    _vm = wrenNewVM(&config);
}

WrenForeignMethodFn detail::VmCallbacks::onBindForeignMethod(::WrenVM* vm, const char* module, const char* className,
                                                             bool isStatic, const char* signature) noexcept {
    try {
        auto* self = static_cast<WrenVM*>(wrenGetUserData(vm));
        if (self == nullptr) {
            return nullptr;
        }

        detail::ForeignMethodKey const key{.module = module != nullptr ? module : "",
                                           .className = className != nullptr ? className : "",
                                           .isStatic = isStatic,
                                           .signature = signature != nullptr ? signature : ""};

        auto it = self->_foreignMethods.find(key);
        if (it != self->_foreignMethods.end()) {
            return it->second;
        }
        return nullptr;
    } catch (...) {
        return nullptr;
    }
}

WrenForeignClassMethods detail::VmCallbacks::onBindForeignClass(::WrenVM* vm, const char* module,
                                                                const char* className) noexcept {
    try {
        auto* self = static_cast<WrenVM*>(wrenGetUserData(vm));
        if (self == nullptr) {
            return WrenForeignClassMethods{.allocate = nullptr, .finalize = nullptr};
        }

        std::string const key =
            std::string(module != nullptr ? module : "") + "/" + (className != nullptr ? className : "");
        auto it = self->_foreignClasses.find(key);
        if (it != self->_foreignClasses.end()) {
            return WrenForeignClassMethods{.allocate = it->second.allocate, .finalize = it->second.finalize};
        }
        return WrenForeignClassMethods{.allocate = nullptr, .finalize = nullptr};
    } catch (...) {
        return WrenForeignClassMethods{.allocate = nullptr, .finalize = nullptr};
    }
}

WrenLoadModuleResult detail::VmCallbacks::onLoadModule(::WrenVM* vm, const char* name) noexcept {
    try {
        auto* self = static_cast<WrenVM*>(wrenGetUserData(vm));
        if (self == nullptr || !self->_moduleLoader) {
            return WrenLoadModuleResult{.source = nullptr, .onComplete = nullptr, .userData = nullptr};
        }

        std::string const moduleName(name != nullptr ? name : "");
        auto it = self->_loadedSources.find(moduleName);
        if (it == self->_loadedSources.end()) {
            std::string source = self->_moduleLoader(moduleName);
            if (source.empty()) {
                return WrenLoadModuleResult{.source = nullptr, .onComplete = nullptr, .userData = nullptr};
            }
            it = self->_loadedSources.emplace(moduleName, std::move(source)).first;
        }

        return WrenLoadModuleResult{.source = it->second.c_str(), .onComplete = nullptr, .userData = nullptr};
    } catch (...) {
        return WrenLoadModuleResult{.source = nullptr, .onComplete = nullptr, .userData = nullptr};
    }
}

void detail::VmCallbacks::onWrite(::WrenVM* vm, const char* text) noexcept {
    try {
        auto* self = static_cast<WrenVM*>(wrenGetUserData(vm));
        std::string_view const chunk(text != nullptr ? text : "");
        if (self != nullptr && self->_writeHandler) {
            self->_writeHandler(chunk);
            return;
        }
        if (self == nullptr) {
            sq::engine::log::info(detail::LOG_TAG, chunk);
            return;
        }
        // `System.print` writes the text, then "\n" on its own: one log line per complete line.
        self->_pendingOutput += chunk;
        if (self->_pendingOutput.size() > WrenVM::MAX_ERROR_LENGTH) {
            sq::engine::log::info(detail::LOG_TAG, self->_pendingOutput);
            self->_pendingOutput.clear();
            return;
        }
        std::size_t newline = self->_pendingOutput.find('\n');
        while (newline != std::string::npos) {
            sq::engine::log::info(detail::LOG_TAG, std::string_view(self->_pendingOutput).substr(0, newline));
            self->_pendingOutput.erase(0, newline + 1);
            newline = self->_pendingOutput.find('\n');
        }
    } catch (const std::exception& e) {
        sq::engine::log::error(detail::LOG_TAG, "exception in the Wren write callback: ", e.what());
    } catch (...) {
        sq::engine::log::error(detail::LOG_TAG, "unknown exception in the Wren write callback");
    }
}

void detail::VmCallbacks::onError(::WrenVM* vm, WrenErrorType type, const char* module, int line,
                                  const char* message) noexcept {
    try {
        auto* self = static_cast<WrenVM*>(wrenGetUserData(vm));
        if (self == nullptr) {
            return;
        }

        std::string const modStr = (module != nullptr ? module : "?");
        std::string const msgStr = (message != nullptr ? message : "");

        std::string text;
        if (type == WREN_ERROR_COMPILE) {
            text = "[" + modStr + " line " + std::to_string(line) + "] " + msgStr;
        } else if (type == WREN_ERROR_RUNTIME) {
            text = "Runtime Error: " + msgStr;
        } else if (type == WREN_ERROR_STACK_TRACE) {
            text = "in " + modStr + " line " + std::to_string(line) + " in " + msgStr;
        }
        detail::appendErrorLine(self->_lastError, text);

        if (self->_errorHandler) {
            ScriptErrorKind kind = ScriptErrorKind::StackTrace;
            if (type == WREN_ERROR_COMPILE) {
                kind = ScriptErrorKind::Compile;
            } else if (type == WREN_ERROR_RUNTIME) {
                kind = ScriptErrorKind::Runtime;
            }
            self->_errorHandler(kind, modStr, line, msgStr);
        } else {
            // The caller logs `lastError()` once; the pieces are only shown in verbose mode.
            sq::engine::log::debug(detail::LOG_TAG, text);
        }
    } catch (const std::exception& e) {
        sq::engine::log::error(detail::LOG_TAG, "exception in the Wren error callback: ", e.what());
    } catch (...) {
        sq::engine::log::error(detail::LOG_TAG, "unknown exception in the Wren error callback");
    }
}

}  // namespace sq::scripting
