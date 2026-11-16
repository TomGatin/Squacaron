/**
 * @file ScriptHotReloader.hpp
 * @brief Watches the script files, checks a modified one in a staging VM, and replays the modules into a new VM.
 */

#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace scripting {

class WrenVM;

/** @brief Seconds between two checks of the watched files on disk. */
inline constexpr float HOT_RELOAD_POLL_INTERVAL = 0.5F;

/**
 * @brief The file side of hot reload: what is watched, what changed, and what to run again.
 *
 * The `ScriptSystem` drives the VM side: it releases its handles, resets the VM, calls `replay()` and builds the
 * script instances again.
 */
class ScriptHotReloader {
  public:
    /** @brief Prepares a fresh VM like the live one (registry, loader, bindings), before a script runs in it. */
    using PrepareVm = std::function<void(WrenVM& vm)>;

    /** @brief A module whose file changed, with its new source. */
    using ModifiedModule = std::pair<std::string, std::string>;

    /**
     * @brief Enables or disables the polling of the files.
     * @param enabled True to poll.
     */
    void enable(bool enabled) noexcept { _enabled = enabled; }

    /**
     * @brief Tells whether the polling is enabled.
     * @return True if enabled (the default).
     */
    [[nodiscard]] bool isEnabled() const noexcept { return _enabled; }

    /**
     * @brief Advances the poll timer.
     * @param dt Elapsed seconds.
     * @return True when the files should be checked now (every `HOT_RELOAD_POLL_INTERVAL`, while enabled).
     */
    bool pollDue(float dt) noexcept;

    /**
     * @brief Starts (or restarts) watching the file of a module. Its last good source is forgotten until
     * `setGoodSource()`.
     * @param module Module name.
     * @param filePath File of the module.
     * @param lastWrite Its modification time now.
     */
    void watch(const std::string& module, std::string filePath, std::filesystem::file_time_type lastWrite);

    /**
     * @brief Records the source a module ran without error, to run it again after a VM reset.
     * @param module Module name.
     * @param filePath File of the module, used if it is not watched yet.
     * @param source Its source.
     */
    void setGoodSource(const std::string& module, const std::string& filePath, std::string source);

    /**
     * @brief Tells whether a file is watched.
     * @return True if at least one module is watched.
     */
    [[nodiscard]] bool hasWatches() const noexcept { return !_watched.empty(); }

    /**
     * @brief Reads the watched files that changed on disk, and keeps those that run in a staging VM.
     *
     * A file that does not run is logged and skipped: the previous version keeps running, until the file changes
     * again.
     * @param prepare Prepares each staging VM.
     * @return The modules to load again, with their new source.
     */
    [[nodiscard]] std::vector<ModifiedModule> collectModified(const PrepareVm& prepare);

    /**
     * @brief Runs the last good source of every watched module again, in a new VM.
     * @param vm The new VM.
     * @return True if every module ran without error; also kept for `lastReplaySucceeded()`.
     */
    bool replay(WrenVM& vm);

    /**
     * @brief Tells whether every module ran again without error in the last `replay()`.
     * @return True when the last replay succeeded, or before any.
     */
    [[nodiscard]] bool lastReplaySucceeded() const noexcept { return _lastReplaySucceeded; }

  private:
    /** @brief A watched script file. */
    struct FileWatch {
        /** @brief File of the module. */
        std::string filePath;
        /** @brief Modification time when it was last read. */
        std::filesystem::file_time_type lastWriteTime;
        /** @brief Last source that ran without error; empty if none. */
        std::string lastGoodSource;
    };

    /**
     * @brief Runs a module source in a staging VM to check it before the live VM is replaced.
     * @param module Module name.
     * @param source Source to check.
     * @param prepare Prepares the staging VM.
     * @return True if it runs.
     */
    static bool validate(std::string_view module, std::string_view source, const PrepareVm& prepare);

    /** @brief Watched files, by module name. */
    std::unordered_map<std::string, FileWatch> _watched;
    /** @brief Time since the last poll. */
    float _pollTimer = 0.0F;
    /** @brief Whether the files are polled. */
    bool _enabled = true;
    /** @brief Result of the last `replay()`. */
    bool _lastReplaySucceeded = true;
};

}  // namespace scripting
