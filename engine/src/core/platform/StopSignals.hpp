#pragma once

/**
 * @file StopSignals.hpp
 * @brief Portable "the user asked the program to stop" flag: SIGINT / SIGTERM, Ctrl+C and console close on Windows.
 */

namespace engine::platform {

/**
 * @brief Installs the handlers that set the stop flag. Idempotent; call it from the main thread.
 *
 * POSIX: SIGINT and SIGTERM. Windows: SIGINT (Ctrl+C), plus Ctrl+Break and closing the console window through a
 * console control handler (Windows never raises SIGTERM). The handlers only set an atomic flag, read with
 * `stopSignalReceived()`: the program keeps control of its shutdown.
 */
void installStopSignalHandlers();

/**
 * @brief Whether a stop signal arrived since the handlers were installed or `clearStopSignal()` was called.
 * @return True once the user asked the program to stop.
 */
[[nodiscard]] bool stopSignalReceived() noexcept;

/** @brief Forgets a received stop signal, e.g. in tests or before running a second loop. */
void clearStopSignal() noexcept;

}  // namespace engine::platform
