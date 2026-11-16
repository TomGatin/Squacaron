/**
 * @file FontStore.hpp
 * @brief Owns the fonts loaded through IScreenDrawService.
 */

#pragma once

#include <memory>
#include <string>

#include "core/services/IScreenDrawService.hpp"

namespace render {

/**
 * @brief Loads fonts and hands them out by `engine::FontId`.
 *
 * Mirrors `TextureStore`: fonts can only be loaded once a `Window` exists, and the store must be destroyed before
 * that window. `engine::NO_FONT` is never stored: callers fall back to raylib's default font for it.
 */
class FontStore {
  public:
    /** @brief Creates an empty store. */
    FontStore();

    /** @brief Unloads every font still held. */
    ~FontStore();

    /** @brief Deleted copy constructor: fonts are unique GPU resources. */
    FontStore(const FontStore&) = delete;
    /** @brief Deleted copy assignment operator. */
    FontStore& operator=(const FontStore&) = delete;
    /** @brief Deleted move constructor. */
    FontStore(FontStore&&) = delete;
    /** @brief Deleted move assignment operator. */
    FontStore& operator=(FontStore&&) = delete;

    /**
     * @brief Loads a font from disk.
     * @param path Font file path (.ttf, .otf, …).
     * @param preferred Identifier to give back if it is free (the one this font had before it was released), or
     * `engine::NO_FONT` for a new one.
     * @return The identifier, or `engine::NO_FONT` if the file could not be loaded.
     */
    engine::FontId load(const std::string& path, engine::FontId preferred = engine::NO_FONT);

    /**
     * @brief Keeps every identifier up to `last` for loads that ask for it as `preferred`.
     * @param last Highest identifier to keep.
     */
    void reserve(engine::FontId last);

    /**
     * @brief Tells whether an identifier names a loaded font.
     * @param id Identifier returned by `load`.
     * @return False for `engine::NO_FONT`, an unknown or an unloaded id.
     */
    [[nodiscard]] bool contains(engine::FontId id) const;

    /** @brief The fonts, defined in `render/detail/RaylibStores.hpp` for the render module only. */
    struct Impl;

    /**
     * @brief Gets the fonts, for the render module.
     * @return The storage.
     */
    [[nodiscard]] const Impl& impl() const noexcept { return *_impl; }

    /**
     * @brief Releases a font; its identifier is never given to another font, and now resolves to nothing.
     *
     * Unknown identifiers and `engine::NO_FONT` are ignored.
     * @param id Identifier returned by `load`.
     */
    void unload(engine::FontId id);

  private:
    /** @brief Fonts. */
    std::unique_ptr<Impl> _impl;
};

}  // namespace render
