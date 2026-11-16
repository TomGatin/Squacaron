#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "components/Renderable.hpp"

namespace render {

/** @brief How a texture is sampled when drawn bigger or smaller than its pixel size. */
enum class TextureFilter : std::uint8_t {
    /** @brief Nearest pixel: crisp pixel art. */
    Nearest,
    /** @brief Mipmapped trilinear filtering, clamped edges: smooth artwork that stays clean when zoomed in or out. */
    Smooth,
};

/**
 * @brief Loads textures and hands them out by `ecs::TextureId`.
 *
 * Minimal owner of GPU textures for the rendering module. Textures can only be loaded once a `Window`
 * exists, and the store must be destroyed before that window.
 */
class TextureStore {
  public:
    /** @brief Creates an empty store. */
    TextureStore();

    /** @brief Unloads every texture still held. */
    ~TextureStore();

    /** @brief Deleted copy constructor: textures are unique GPU resources. */
    TextureStore(const TextureStore&) = delete;
    /** @brief Deleted copy assignment operator. */
    TextureStore& operator=(const TextureStore&) = delete;
    /** @brief Deleted move constructor. */
    TextureStore(TextureStore&&) = delete;
    /** @brief Deleted move assignment operator. */
    TextureStore& operator=(TextureStore&&) = delete;

    /**
     * @brief Loads a texture from disk.
     *
     * @param path Image file path.
     * @param filter Sampling used when the texture is scaled.
     * @param preferred Identifier to give back if it is free (the one this texture had before it was released), or
     * `ecs::NO_TEXTURE` for a new one.
     * @return The identifier, or `ecs::NO_TEXTURE` if the file could not be loaded.
     */
    ecs::TextureId load(const std::string& path, TextureFilter filter = TextureFilter::Nearest,
                        ecs::TextureId preferred = ecs::NO_TEXTURE);

    /**
     * @brief Keeps every identifier up to `last` for loads that ask for it as `preferred`.
     * @param last Highest identifier to keep.
     */
    void reserve(ecs::TextureId last);

    /**
     * @brief Tells whether an identifier names a loaded texture.
     * @param id Identifier returned by `load`.
     * @return False for `ecs::NO_TEXTURE`, an unknown or an unloaded id.
     */
    [[nodiscard]] bool contains(ecs::TextureId id) const;

    /** @brief The GPU textures, defined in `render/detail/RaylibStores.hpp` for the render module only. */
    struct Impl;

    /**
     * @brief Gets the GPU textures, for the render module.
     * @return The storage.
     */
    [[nodiscard]] const Impl& impl() const noexcept { return *_impl; }

    /**
     * @brief Releases a texture; its identifier is never given to another texture, and now resolves to nothing.
     *
     * Unknown identifiers, `ecs::NO_TEXTURE` and the placeholder are ignored.
     * @param id Identifier returned by `load`.
     */
    void unload(ecs::TextureId id);

    /**
     * @brief Gets the texture drawn in place of a missing file: a magenta and black checkerboard, made on first use.
     * @return Its identifier, or `ecs::NO_TEXTURE` if it cannot be created (no window).
     */
    ecs::TextureId placeholder();

    /**
     * @brief Counts the textures held, the placeholder included.
     * @return Number of loaded textures.
     */
    [[nodiscard]] std::size_t loadedCount() const noexcept;

  private:
    /** @brief GPU textures. */
    std::unique_ptr<Impl> _impl;
};

}  // namespace render
