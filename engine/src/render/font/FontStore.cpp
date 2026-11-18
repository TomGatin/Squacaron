#include "render/font/FontStore.hpp"

#include <raylib.h>

#include <cstddef>
#include <memory>
#include <string>

#include "render/api/IScreenDrawService.hpp"
#include "render/detail/RaylibStores.hpp"
#include "utils/StableSlots.hpp"

namespace sq::render {

FontStore::FontStore() : _impl(std::make_unique<Impl>()) {}

FontStore::~FontStore() {
    if (!IsWindowReady()) {
        return;
    }
    for (const Impl::Slot& slot : _impl->fonts) {
        if (slot.loaded) {
            UnloadFont(slot.font);
        }
    }
}

namespace {

/**
 * @brief Pixel size the glyphs are rasterized at: big enough for large titles, smoothly scaled down for small text
 * (raylib's default, 32, turns blurry as soon as a text is drawn bigger).
 */
constexpr int GLYPH_RASTER_SIZE = 128;

}  // namespace

sq::engine::FontId FontStore::load(const std::string& path, const sq::engine::FontId preferred) {
    Font font = LoadFontEx(path.c_str(), GLYPH_RASTER_SIZE, nullptr, 0);
    // raylib falls back to its default font when the file cannot be read: that one is not ours to unload.
    if (font.texture.id == 0 || font.texture.id == GetFontDefault().texture.id) {
        return sq::engine::NO_FONT;
    }
    GenTextureMipmaps(&font.texture);
    SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
    return sq::ecs::placeInSlot(_impl->fonts, Impl::Slot{.font = font, .loaded = true}, preferred);
}

void FontStore::reserve(const sq::engine::FontId last) { sq::ecs::reserveSlots(_impl->fonts, last); }

bool FontStore::contains(sq::engine::FontId id) const { return _impl->get(id) != nullptr; }

void FontStore::unload(const sq::engine::FontId id) {
    if (id == sq::engine::NO_FONT || static_cast<std::size_t>(id) > _impl->fonts.size()) {
        return;
    }
    Impl::Slot& slot = _impl->fonts.at(id - 1);
    if (!slot.loaded) {
        return;
    }
    if (IsWindowReady()) {
        UnloadFont(slot.font);
    }
    slot = Impl::Slot{};
}

}  // namespace sq::render
