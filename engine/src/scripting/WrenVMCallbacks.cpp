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

namespace scripting {

namespace detail {

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

}  // namespace detail

WrenForeignMethodFn WrenVM::onBindForeignMethod(::WrenVM* vm, const char* module, const char* className, bool isStatic,
                                                const char* signature) noexcept {
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

WrenForeignClassMethods WrenVM::onBindForeignClass(::WrenVM* vm, const char* module, const char* className) noexcept {
    try {
        auto* self = static_cast<WrenVM*>(wrenGetUserData(vm));
        if (self == nullptr) {
            return WrenForeignClassMethods{.allocate = nullptr, .finalize = nullptr};
        }

        std::string const key =
            std::string(module != nullptr ? module : "") + "/" + (className != nullptr ? className : "");
        auto it = self->_foreignClasses.find(key);
        if (it != self->_foreignClasses.end()) {
            return it->second;
        }
        return WrenForeignClassMethods{.allocate = nullptr, .finalize = nullptr};
    } catch (...) {
        return WrenForeignClassMethods{.allocate = nullptr, .finalize = nullptr};
    }
}

WrenLoadModuleResult WrenVM::onLoadModule(::WrenVM* vm, const char* name) noexcept {
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

void WrenVM::onWrite(::WrenVM* vm, const char* text) noexcept {
    try {
        auto* self = static_cast<WrenVM*>(wrenGetUserData(vm));
        std::string_view const chunk(text != nullptr ? text : "");
        if (self != nullptr && self->_writeHandler) {
            self->_writeHandler(chunk);
            return;
        }
        if (self == nullptr) {
            engine::log::info(detail::LOG_TAG, chunk);
            return;
        }
        // `System.print` writes the text, then "\n" on its own: one log line per complete line.
        self->_pendingOutput += chunk;
        if (self->_pendingOutput.size() > MAX_ERROR_LENGTH) {
            engine::log::info(detail::LOG_TAG, self->_pendingOutput);
            self->_pendingOutput.clear();
            return;
        }
        std::size_t newline = self->_pendingOutput.find('\n');
        while (newline != std::string::npos) {
            engine::log::info(detail::LOG_TAG, std::string_view(self->_pendingOutput).substr(0, newline));
            self->_pendingOutput.erase(0, newline + 1);
            newline = self->_pendingOutput.find('\n');
        }
    } catch (const std::exception& e) {
        engine::log::error(detail::LOG_TAG, "exception in the Wren write callback: ", e.what());
    } catch (...) {
        engine::log::error(detail::LOG_TAG, "unknown exception in the Wren write callback");
    }
}

void WrenVM::onError(::WrenVM* vm, WrenErrorType type, const char* module, int line, const char* message) noexcept {
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
            self->_errorHandler(type, modStr, line, msgStr);
        } else {
            // The caller logs `lastError()` once; the pieces are only shown in verbose mode.
            engine::log::debug(detail::LOG_TAG, text);
        }
    } catch (const std::exception& e) {
        engine::log::error(detail::LOG_TAG, "exception in the Wren error callback: ", e.what());
    } catch (...) {
        engine::log::error(detail::LOG_TAG, "unknown exception in the Wren error callback");
    }
}

}  // namespace scripting
