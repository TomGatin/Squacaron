/**
 * @file ScriptHotReloader.cpp
 * @brief Implementation of the file watching, staging checks and module replay of hot reload.
 */

#include "scripting/ScriptHotReloader.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "log/Logger.hpp"
#include "scripting/WrenVM.hpp"
#include "scripting/detail/LogTag.hpp"
#include "scripting/detail/ScriptSupport.hpp"
#include "utils/Paths.hpp"

namespace sq::scripting {

using detail::LOG_TAG;

bool ScriptHotReloader::pollDue(const float dt) noexcept {
    if (!_enabled) {
        return false;
    }
    _pollTimer += dt;
    if (_pollTimer < HOT_RELOAD_POLL_INTERVAL) {
        return false;
    }
    _pollTimer = 0.0F;
    return true;
}

void ScriptHotReloader::watch(const std::string& module, std::string filePath,
                              const std::filesystem::file_time_type lastWrite) {
    _watched[module] = FileWatch{.filePath = std::move(filePath), .lastWriteTime = lastWrite, .lastGoodSource = ""};
}

void ScriptHotReloader::setGoodSource(const std::string& module, const std::string& filePath, std::string source) {
    if (auto it = _watched.find(module); it != _watched.end()) {
        it->second.lastGoodSource = std::move(source);
        return;
    }
    std::error_code ec;
    auto const lastWrite = std::filesystem::last_write_time(filePath, ec);
    _watched[module] = FileWatch{.filePath = filePath,
                                 .lastWriteTime = ec ? std::filesystem::file_time_type{} : lastWrite,
                                 .lastGoodSource = std::move(source)};
}

bool ScriptHotReloader::validate(const std::string_view module, const std::string_view source,
                                 const PrepareVm& prepare) {
    WrenVM stagingVm;
    prepare(stagingVm);
    auto res = stagingVm.interpret(module, source);
    if (!res) {
        sq::engine::log::warning(
            LOG_TAG, "hot reload: script module ", sq::utils::quoteForLog(module),
            " ignored, the previous version keeps running: ", detail::formatScriptError(res.error()));
        return false;
    }
    return true;
}

std::vector<ScriptHotReloader::ModifiedModule> ScriptHotReloader::collectModified(const PrepareVm& prepare) {
    std::vector<ModifiedModule> modified;
    std::error_code ec;

    for (auto& [module, watch] : _watched) {
        if (!std::filesystem::exists(watch.filePath, ec)) {
            continue;
        }
        auto const currentWriteTime = std::filesystem::last_write_time(watch.filePath, ec);
        if (ec || currentWriteTime == watch.lastWriteTime) {
            continue;
        }

        std::ifstream file(watch.filePath);
        if (!file.is_open()) {
            continue;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string newSource = buffer.str();

        watch.lastWriteTime = currentWriteTime;
        if (!validate(module, newSource, prepare)) {
            continue;
        }
        watch.lastGoodSource = newSource;
        modified.emplace_back(module, std::move(newSource));
    }
    return modified;
}

bool ScriptHotReloader::replay(WrenVM& vm) {
    _lastReplaySucceeded = true;
    for (auto const& [module, watch] : _watched) {
        if (watch.lastGoodSource.empty()) {
            continue;
        }
        auto const res = vm.interpret(module, watch.lastGoodSource);
        if (!res) {
            _lastReplaySucceeded = false;
            sq::engine::log::warning(LOG_TAG, "hot reload: script module ", sq::utils::quoteForLog(module),
                                     " failed: ", detail::formatScriptError(res.error()));
        }
    }
    return _lastReplaySucceeded;
}

}  // namespace sq::scripting
