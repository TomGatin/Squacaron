/**
 * @file RaylibStores.hpp
 * @brief raylib side of `TextureStore` and `FontStore`. Private to the render module: not installed.
 */

#pragma once

#include <raylib.h>

#include <cstddef>
#include <vector>

#include "render/api/IScreenDrawService.hpp"
#include "render/api/components/Renderable.hpp"
#include "render/font/FontStore.hpp"
#include "render/texture/TextureStore.hpp"

namespace sq::render {

/** @brief GPU textures of a `TextureStore`. */
struct TextureStore::Impl {
    /** @brief One texture. */
    struct Slot {
        /** @brief raylib texture. */
        Texture2D texture{};
        /** @brief False once unloaded; the slot is kept so its identifier is never reused. */
        bool loaded = false;
    };

    /** @brief Textures; identifier `n` is stored at index `n - 1`. */
    std::vector<Slot> textures;
    /** @brief Identifier of the placeholder, once made. */
    sq::ecs::TextureId placeholder = sq::ecs::NO_TEXTURE;

    /**
     * @brief Looks a texture up.
     * @param id Identifier returned by `TextureStore::load`.
     * @return The texture, or nullptr if `id` is unknown or unloaded.
     */
    [[nodiscard]] const Texture2D* get(sq::ecs::TextureId id) const {
        if (id == sq::ecs::NO_TEXTURE || static_cast<std::size_t>(id) > textures.size()) {
            return nullptr;
        }
        const Slot& slot = textures.at(id - 1);
        return slot.loaded ? &slot.texture : nullptr;
    }
};

/** @brief Fonts of a `FontStore`. */
struct FontStore::Impl {
    /** @brief One font. */
    struct Slot {
        /** @brief raylib font. */
        Font font{};
        /** @brief False once unloaded; the slot is kept so its identifier is never reused. */
        bool loaded = false;
    };

    /** @brief Fonts; identifier `n` is stored at index `n - 1`. */
    std::vector<Slot> fonts;

    /**
     * @brief Looks a font up.
     * @param id Identifier returned by `FontStore::load`.
     * @return The font, or nullptr for `sq::engine::NO_FONT` or an unknown id.
     */
    [[nodiscard]] const Font* get(sq::engine::FontId id) const {
        if (id == sq::engine::NO_FONT || static_cast<std::size_t>(id) > fonts.size()) {
            return nullptr;
        }
        const Slot& slot = fonts.at(id - 1);
        return slot.loaded ? &slot.font : nullptr;
    }
};

}  // namespace sq::render
