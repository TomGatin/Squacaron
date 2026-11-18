#include "render/texture/TextureStore.hpp"

#include <raylib.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>

#include "render/api/components/Renderable.hpp"
#include "render/detail/RaylibStores.hpp"
#include "utils/StableSlots.hpp"

namespace sq::render {

namespace {

/** @brief Side of the placeholder texture, in pixels. */
constexpr int PLACEHOLDER_SIZE = 64;
/** @brief Side of one square of the placeholder checkerboard, in pixels. */
constexpr int PLACEHOLDER_SQUARE = 16;
/** @brief Magenta squares of the placeholder: a color no real asset uses, so a missing file is obvious. */
constexpr Color PLACEHOLDER_MAGENTA = {.r = 255, .g = 0, .b = 255, .a = 255};
/** @brief Dark squares of the placeholder. */
constexpr Color PLACEHOLDER_DARK = {.r = 0, .g = 0, .b = 0, .a = 255};

}  // namespace

TextureStore::TextureStore() : _impl(std::make_unique<Impl>()) {}

TextureStore::~TextureStore() {
    if (!IsWindowReady()) {
        return;
    }

    for (const Impl::Slot& slot : _impl->textures) {
        if (slot.loaded) {
            UnloadTexture(slot.texture);
        }
    }
}

sq::ecs::TextureId TextureStore::load(const std::string& path, const TextureFilter filter,
                                      const sq::ecs::TextureId preferred) {
    Texture2D texture = LoadTexture(path.c_str());
    if (texture.id == 0) {
        return sq::ecs::NO_TEXTURE;
    }
    if (filter == TextureFilter::Smooth) {
        GenTextureMipmaps(&texture);
        SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
        // Clamped: the filter must not blend an edge with the opposite one (seams between tiled pictures).
        SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
    }
    return sq::ecs::placeInSlot(_impl->textures, Impl::Slot{.texture = texture, .loaded = true}, preferred);
}

void TextureStore::reserve(const sq::ecs::TextureId last) { sq::ecs::reserveSlots(_impl->textures, last); }

bool TextureStore::contains(sq::ecs::TextureId id) const { return _impl->get(id) != nullptr; }

void TextureStore::unload(const sq::ecs::TextureId id) {
    if (id == sq::ecs::NO_TEXTURE || id == _impl->placeholder ||
        static_cast<std::size_t>(id) > _impl->textures.size()) {
        return;
    }
    Impl::Slot& slot = _impl->textures.at(id - 1);
    if (!slot.loaded) {
        return;
    }
    if (IsWindowReady()) {
        UnloadTexture(slot.texture);
    }
    slot = Impl::Slot{};
}

sq::ecs::TextureId TextureStore::placeholder() {
    if (_impl->placeholder != sq::ecs::NO_TEXTURE || !IsWindowReady()) {
        return _impl->placeholder;
    }
    const Image image = GenImageChecked(PLACEHOLDER_SIZE, PLACEHOLDER_SIZE, PLACEHOLDER_SQUARE, PLACEHOLDER_SQUARE,
                                        PLACEHOLDER_MAGENTA, PLACEHOLDER_DARK);
    const Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if (texture.id == 0) {
        return sq::ecs::NO_TEXTURE;
    }
    _impl->textures.push_back(Impl::Slot{.texture = texture, .loaded = true});
    _impl->placeholder = static_cast<sq::ecs::TextureId>(_impl->textures.size());
    return _impl->placeholder;
}

std::size_t TextureStore::loadedCount() const noexcept {
    return static_cast<std::size_t>(
        std::ranges::count_if(_impl->textures, [](const Impl::Slot& slot) { return slot.loaded; }));
}

}  // namespace sq::render
