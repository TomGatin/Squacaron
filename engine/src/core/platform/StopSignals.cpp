#include "core/platform/StopSignals.hpp"

#include <atomic>
#include <csignal>

#ifdef _WIN32
#include <windows.h>
#endif

namespace engine::platform {

namespace {

// Written by a signal handler: it must be lock-free to be signal-safe.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<bool> g_stopRequested{false};
static_assert(std::atomic<bool>::is_always_lock_free);

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<bool> g_installed{false};

extern "C" void onStopSignal(int /*signal*/) { g_stopRequested.store(true, std::memory_order_relaxed); }

#ifdef _WIN32
BOOL WINAPI onConsoleControl(const DWORD event) {
    switch (event) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            g_stopRequested.store(true, std::memory_order_relaxed);
            return TRUE;
        default:
            return FALSE;
    }
}
#endif

}  // namespace

void installStopSignalHandlers() {
    if (g_installed.exchange(true)) {
        return;
    }
    std::signal(SIGINT, onStopSignal);
    std::signal(SIGTERM, onStopSignal);
#ifdef _WIN32
    SetConsoleCtrlHandler(onConsoleControl, TRUE);
#endif
}

bool stopSignalReceived() noexcept { return g_stopRequested.load(std::memory_order_relaxed); }

void clearStopSignal() noexcept { g_stopRequested.store(false, std::memory_order_relaxed); }

}  // namespace engine::platform
