#include "assets/render/RenderAssetsPlugin.hpp"

#include <string>

#include "assets/AssetKinds.hpp"
#include "assets/AssetManager.hpp"
#include "assets/AssetPlugin.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Registry.hpp"
#include "render/RenderPlugin.hpp"
#include "render/api/IScreenDrawService.hpp"
#include "render/api/components/Renderable.hpp"
#include "render/font/FontStore.hpp"
#include "render/screen/ScreenDrawService.hpp"
#include "render/texture/TextureStore.hpp"

namespace sq::assets {

void RenderAssetsPlugin::build(sq::engine::PluginBuilder& builder) {
    builder.require<sq::render::RenderPlugin>();
    builder.require<AssetPlugin>();
}

void RenderAssetsPlugin::init(sq::engine::Engine& engine) {
    auto& renderPlugin = engine.getPlugin<sq::render::RenderPlugin>();
    sq::render::TextureStore* textures = renderPlugin.textures();
    sq::render::ScreenDrawService* screen = renderPlugin.screenDraw();
    if (textures == nullptr || screen == nullptr) {
        return;
    }
    auto& manager = engine.registry().getResource<AssetManager>();
    manager.setLoader<Texture>(AssetLoader<Texture>{
        .load =
            [textures](const std::string& path, const LoadOptions& options, const sq::ecs::TextureId preferred) {
                const sq::render::TextureFilter filter = options.filter == TextureFilter::Smooth
                                                             ? sq::render::TextureFilter::Smooth
                                                             : sq::render::TextureFilter::Nearest;
                return textures->load(path, filter, preferred);
            },
        .unload = [textures](const sq::ecs::TextureId id) { textures->unload(id); },
        .fallback = [textures]() { return textures->placeholder(); },
        .reserve = [textures](const sq::ecs::TextureId last) { textures->reserve(last); },
    });
    sq::render::FontStore* fonts = &screen->fonts();
    manager.setLoader<Font>(AssetLoader<Font>{
        .load = [fonts](const std::string& path, const LoadOptions& /*options*/,
                        const sq::engine::FontId preferred) { return fonts->load(path, preferred); },
        .unload = [fonts](const sq::engine::FontId id) { fonts->unload(id); },
        // No fallback: `NO_FONT` already draws with the backend's default font.
        .fallback = {},
        .reserve = [fonts](const sq::engine::FontId last) { fonts->reserve(last); },
    });
    _attached = true;
}

void RenderAssetsPlugin::shutdown(sq::engine::Engine& engine) {
    if (!_attached) {
        return;
    }
    _attached = false;
    if (!engine.registry().hasResource<AssetManager>()) {
        return;
    }
    auto& manager = engine.registry().getResource<AssetManager>();
    manager.detachLoader<Texture>();
    manager.detachLoader<Font>();
}

}  // namespace sq::assets
