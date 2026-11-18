/**
 * @file KeyNames.hpp
 * @brief Names of the keyboard keys: a stable id to save a binding, and a label to show it in a menu.
 */

#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "input/KeyboardLayout.hpp"
#include "render/api/InputCodes.hpp"

namespace sq::input {

/** @brief One keyboard key and its saved id. */
struct KeyName {
    /** @brief The key (a physical position, named after the US QWERTY layout). */
    sq::engine::Key key = sq::engine::Key::Unknown;
    /** @brief Id written to the settings file, e.g. "Space" or "LeftShift". */
    std::string_view id;
};

/**
 * @brief Every key a controls screen can bind, in enum order.
 * @return The keys and their ids; `sq::engine::Key::Unknown` is not in the list.
 */
[[nodiscard]] std::span<const KeyName> keyNames() noexcept;

/**
 * @brief Gets the saved id of a key.
 * @param key Key.
 * @return Its id, or an empty view for `sq::engine::Key::Unknown`.
 */
[[nodiscard]] std::string_view keyId(sq::engine::Key key) noexcept;

/**
 * @brief Reads a saved key id.
 * @param id Id written by `keyId`.
 * @return The key, or nothing if the id is unknown.
 */
[[nodiscard]] std::optional<sq::engine::Key> keyFromId(std::string_view id) noexcept;

/**
 * @brief Gets the label of a key as printed on the player's keyboard.
 *
 * Keys are physical positions named after QWERTY: on AZERTY the key at the position of `Key::W` is labelled "Z", so
 * the letters that move are swapped back for the label.
 *
 * @param key Key.
 * @param layout Layout of the player's keyboard.
 * @return Upper-case label, e.g. "Z", "SPACE" or "LEFT SHIFT"; "NONE" for `sq::engine::Key::Unknown`.
 */
[[nodiscard]] std::string keyLabel(sq::engine::Key key, KeyboardLayout layout);

}  // namespace sq::input
