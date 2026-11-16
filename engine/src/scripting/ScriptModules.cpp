/**
 * @file ScriptModules.cpp
 * @brief Script modules: the import resolver, the files under the script root, and the error formatting.
 */

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "assets/AssetPath.hpp"
#include "log/Logger.hpp"
#include "scripting/CoreModules.hpp"
#include "scripting/ScriptSystem.hpp"
#include "scripting/WrenVM.hpp"
#include "scripting/detail/LogTag.hpp"
#include "scripting/detail/ScriptSupport.hpp"

namespace scripting {

using detail::LOG_TAG;

namespace {

/** @brief Extension of the script files. */
constexpr std::string_view SCRIPT_EXTENSION = ".wren";

/** @brief Reads a whole file; empty if it cannot be opened. */
std::optional<std::string> readFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

}  // namespace

namespace detail {

std::string formatScriptError(std::string_view error) {
    std::string text;
    while (!error.empty()) {
        std::size_t const end = error.find('\n');
        std::string_view line = error.substr(0, end);
        error = end == std::string_view::npos ? std::string_view{} : error.substr(end + 1);
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
            line.remove_prefix(1);
        }
        if (line.empty()) {
            continue;
        }
        if (!text.empty()) {
            text += " | ";
        }
        text += line;
    }
    return text.empty() ? std::string("unknown error") : text;
}

std::optional<std::filesystem::path> resolveScriptFile(const std::filesystem::path& root, std::string_view moduleName) {
    std::optional<std::string> normalized = assets::normalizeAssetPath(moduleName);
    if (!normalized || !assets::isConfinedAssetPath(*normalized)) {
        return std::nullopt;
    }
    if (!normalized->ends_with(SCRIPT_EXTENSION)) {
        *normalized += SCRIPT_EXTENSION;
    }
    std::filesystem::path const file = assets::fromUtf8(*normalized);
    return root.empty() ? file : root / file;
}

void logRefusedScriptModule(std::string_view moduleName) {
    engine::log::warning(LOG_TAG, "script module ", assets::quoteForLog(moduleName),
                         " refused: it must be a relative path inside the script root, without '..'");
}

}  // namespace detail

void ScriptSystem::installModuleLoader() {
    // The system is not movable: the loader, copied into the staging VMs, can read its modules and root.
    _vm.setModuleLoader([this](std::string_view moduleName) -> std::string {
        std::string_view const coreSource = getCoreModuleSource(moduleName);
        if (!coreSource.empty()) {
            return std::string(coreSource);
        }
        if (auto const it = _extraModules.find(std::string(moduleName)); it != _extraModules.end()) {
            return it->second;
        }
        std::optional<std::filesystem::path> const filename = detail::resolveScriptFile(_scriptRoot, moduleName);
        if (!filename) {
            detail::logRefusedScriptModule(moduleName);
            return "";
        }
        return readFile(*filename).value_or("");
    });

    auto const res = _vm.interpret("_internal", INTERNAL_DISPATCHER_SOURCE);
    if (!res) {
        engine::log::error(LOG_TAG, "cannot run the internal dispatcher: ", detail::formatScriptError(res.error()));
    }
}

bool ScriptSystem::ensureScriptModuleLoaded(const std::string& moduleName) {
    std::optional<std::filesystem::path> const resolved = detail::resolveScriptFile(_scriptRoot, moduleName);
    if (!resolved) {
        if (_errors.reportOnce(moduleName)) {
            detail::logRefusedScriptModule(moduleName);
        }
        return false;
    }
    std::string const filename = assets::toUtf8(*resolved);

    std::error_code ec;
    if (std::filesystem::exists(filename, ec)) {
        auto const lastWrite = std::filesystem::last_write_time(filename, ec);
        if (!ec) {
            _reloader.watch(moduleName, filename, lastWrite);
        }
    }
    if (_vm.hasModule(moduleName)) {
        return true;
    }

    std::optional<std::string> source = readFile(filename);
    if (!source) {
        if (_errors.reportOnce(moduleName)) {
            engine::log::warning(LOG_TAG, "script module ", assets::quoteForLog(moduleName), ": file ",
                                 assets::quoteForLog(filename, assets::MAX_PATH_LENGTH), " not found");
        }
        return false;
    }
    auto res = _vm.interpret(moduleName, *source);
    if (!res) {
        if (_errors.reportOnce(moduleName)) {
            engine::log::warning(LOG_TAG, "cannot load script module ", assets::quoteForLog(moduleName), ": ",
                                 detail::formatScriptError(res.error()));
        }
        return false;
    }
    _reloader.setGoodSource(moduleName, filename, std::move(*source));
    return true;
}

}  // namespace scripting
