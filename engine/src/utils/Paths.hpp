/**
 * @file Paths.hpp
 * @brief Portable path helpers for names that come from games and scripts: normalisation, confinement, UTF-8, logs.
 */

#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace sq::utils {

/** @brief Longest accepted path, in bytes. */
inline constexpr std::size_t MAX_PATH_LENGTH = 1024;

/** @brief Default length `quoteForLog` keeps, in bytes. */
inline constexpr std::size_t DEFAULT_QUOTE_LENGTH = 128;

/**
 * @brief Turns a path into a portable key.
 *
 * Both `/` and `\` are separators, `.` and `..` are folded (`a/../a/x.png` is `a/x.png`) and the result uses `/`, so
 * the same file gets the same key on Linux and Windows. The path is not resolved against the disk.
 *
 * @param path Path given by the game or a script.
 * @return The key, or nothing for an empty, too long or NUL-holding path.
 */
[[nodiscard]] std::optional<std::string> normalizePath(std::string_view path);

/**
 * @brief Tells whether a normalised path stays inside the directory it is relative to.
 * @param normalized Result of `normalizePath`.
 * @return False for an absolute path (`/x`, `C:/x`, `//server/x`) or one that climbs out of it (`../x`).
 */
[[nodiscard]] bool isConfinedPath(std::string_view normalized) noexcept;

/**
 * @brief Converts a path to UTF-8 with `/` separators, whatever the platform's native encoding.
 *
 * `std::filesystem::path::string()` throws or mangles non-ANSI names on Windows; this does not.
 * @param path Path.
 * @return UTF-8 text.
 */
[[nodiscard]] std::string toUtf8(const std::filesystem::path& path);

/**
 * @brief Builds a path from UTF-8 text, whatever the platform's native encoding.
 * @param text UTF-8 path.
 * @return Path.
 */
[[nodiscard]] std::filesystem::path fromUtf8(std::string_view text);

/**
 * @brief Cuts a name or path for a log line, so that a script cannot fill the logs with huge strings.
 * @param text Text to show.
 * @param limit Longest part kept, in bytes.
 * @return The text between quotes, ending with `...` when it was cut.
 */
[[nodiscard]] std::string quoteForLog(std::string_view text, std::size_t limit = DEFAULT_QUOTE_LENGTH);

}  // namespace sq::utils
