/**
 * @file AssetPath.hpp
 * @brief Validation and normalisation of asset paths, aliases and group names.
 */

#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "utils/Paths.hpp"

namespace sq::assets {

/** @brief Longest accepted asset path, in bytes. */
inline constexpr std::size_t MAX_PATH_LENGTH = sq::utils::MAX_PATH_LENGTH;

/** @brief Longest accepted alias or group name, in bytes. */
inline constexpr std::size_t MAX_NAME_LENGTH = 128;

/** @brief Most files `AssetManager::preloadDirectory` takes from one directory. */
inline constexpr std::size_t MAX_DIRECTORY_FILES = 1024;

/**
 * @brief Tells whether a string can be used as a group name.
 * @param name Name to check.
 * @return True for a non-empty name of at most `MAX_NAME_LENGTH` bytes without NUL.
 */
[[nodiscard]] bool isValidAssetName(std::string_view name) noexcept;

/**
 * @brief Tells whether a string can be used as an alias.
 *
 * An alias is a valid name without `/`, `\` or `.`, so that it can never be mistaken for a path (see
 * `looksLikeAssetPath`).
 * @param alias Alias to check.
 * @return True for a valid alias.
 */
[[nodiscard]] bool isValidAssetAlias(std::string_view alias) noexcept;

/**
 * @brief Tells whether a string given where an alias or a path is accepted is a path.
 * @param aliasOrPath Alias or path.
 * @return True if it holds a `/`, a `\` or a `.`; aliases never do.
 */
[[nodiscard]] bool looksLikeAssetPath(std::string_view aliasOrPath) noexcept;

}  // namespace sq::assets
