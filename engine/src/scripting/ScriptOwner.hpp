/**
 * @file ScriptOwner.hpp
 * @brief Identifier of what runs Wren code: a script entity, or an id handed out to an extension.
 */

#pragma once

#include <cstdint>
#include <limits>

namespace scripting {

/**
 * @brief Opaque identifier of what is running Wren code: a script entity, or anything an extension defines.
 *
 * Entity scripts use their entity handle (52 bits). Extensions build theirs with `makeExtensionOwner()`: the top bit
 * is set, so they never collide with an entity.
 */
using ScriptOwner = std::uint64_t;

/** @brief `ScriptOwner` meaning "nobody": code run outside any script or extension call. */
inline constexpr ScriptOwner NO_SCRIPT_OWNER = std::numeric_limits<ScriptOwner>::max();

/** @brief Top bit of a `ScriptOwner` defined by an extension, so it never equals an entity handle. */
inline constexpr ScriptOwner EXTENSION_OWNER_BIT = ScriptOwner{1} << 63U;

/**
 * @brief Builds the owner id of something an extension runs (a scene, a console command...).
 * @param tag Marker of the extension, so two extensions never build the same id (e.g. `0x5CE0` for scenes).
 * @param id Id inside the extension.
 * @return The owner: top bit set, `tag` in bits 32..47, `id` in bits 0..31.
 */
[[nodiscard]] constexpr ScriptOwner makeExtensionOwner(std::uint16_t tag, std::uint32_t id) noexcept {
    return EXTENSION_OWNER_BIT | (ScriptOwner{tag} << 32U) | ScriptOwner{id};
}

/**
 * @brief Tells whether an owner is a script entity (not an extension owner, not `NO_SCRIPT_OWNER`).
 * @param owner Owner to check.
 * @return True if `owner` is an entity handle.
 */
[[nodiscard]] constexpr bool isEntityOwner(ScriptOwner owner) noexcept {
    return (owner & EXTENSION_OWNER_BIT) == 0 && owner != NO_SCRIPT_OWNER;
}

}  // namespace scripting
