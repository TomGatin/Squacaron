/**
 * @file AssetsPlugin.hpp
 * @brief Loads the RLSS textures and font once, for the view plugins.
 */

#pragma once

#include <string_view>
#include <vector>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "render/api/IScreenDrawService.hpp"
#include "render/api/components/Renderable.hpp"

namespace rlss::client {

/** @brief Wren module declaring the RLSS assets (`rlss/scripts/Assets.wren`), read from the content root. */
inline constexpr std::string_view ASSET_DECLARATIONS = "rlss/scripts/Assets";
/** @brief Alias of the font of the menus, the HUD and the name tags. */
inline constexpr std::string_view FONT_ALIAS = "rlss_font";

/** @brief One piece of the arena picture, too big for a single texture. */
struct ArenaTile {
    /** @brief Texture. */
    sq::ecs::TextureId texture = sq::ecs::NO_TEXTURE;
    /** @brief Row, from the top. */
    int row = 0;
    /** @brief Column, from the left. */
    int column = 0;
};

/**
 * @brief Every texture and the font of the game, an ECS resource (`registry.getResource<GameAssets>()`).
 *
 * A texture that cannot be loaded is `sq::ecs::NO_TEXTURE` (logged): the arena and the stadium skip it, a car or ball
 * part using it is drawn as a small flat rectangle, the HUD skips it.
 */
struct GameAssets {
    /** @brief Stands and floodlights, behind the arena (parallax). */
    sq::ecs::TextureId stadium = sq::ecs::NO_TEXTURE;
    /** @brief Arena tiles, row by row. */
    std::vector<ArenaTile> arena;
    /** @brief Car body, greyscale: tinted with the player color. */
    sq::ecs::TextureId carBody = sq::ecs::NO_TEXTURE;
    /** @brief Car glass, trims and lights, drawn untinted over the body. */
    sq::ecs::TextureId carDetails = sq::ecs::NO_TEXTURE;
    /** @brief Wheel. */
    sq::ecs::TextureId wheel = sq::ecs::NO_TEXTURE;
    /** @brief Boost flame. */
    sq::ecs::TextureId flame = sq::ecs::NO_TEXTURE;
    /** @brief Soft round glow, white. */
    sq::ecs::TextureId glow = sq::ecs::NO_TEXTURE;
    /** @brief Boost trail particle, white. */
    sq::ecs::TextureId puff = sq::ecs::NO_TEXTURE;
    /** @brief Ball panels (turn with the ball). */
    sq::ecs::TextureId ball = sq::ecs::NO_TEXTURE;
    /** @brief Ball lighting (does not turn). */
    sq::ecs::TextureId ballShade = sq::ecs::NO_TEXTURE;
    /** @brief HUD: boost meter disc. */
    sq::ecs::TextureId hudDisc = sq::ecs::NO_TEXTURE;
    /** @brief HUD: one tick of the boost meter ring. */
    sq::ecs::TextureId hudTick = sq::ecs::NO_TEXTURE;
    /** @brief HUD: score panel slanted on its left side. */
    sq::ecs::TextureId hudSlantLeft = sq::ecs::NO_TEXTURE;
    /** @brief HUD: score panel slanted on its right side. */
    sq::ecs::TextureId hudSlantRight = sq::ecs::NO_TEXTURE;
    /** @brief HUD: soft horizontal band (banners). */
    sq::ecs::TextureId hudBand = sq::ecs::NO_TEXTURE;
    /** @brief Goal explosion of the blue team: a strip of `EXPLOSION_FRAMES` square frames. */
    sq::ecs::TextureId explosionBlue = sq::ecs::NO_TEXTURE;
    /** @brief Goal explosion of the orange team. */
    sq::ecs::TextureId explosionOrange = sq::ecs::NO_TEXTURE;
    /** @brief Shockwave ring, white. */
    sq::ecs::TextureId shockwave = sq::ecs::NO_TEXTURE;
    /** @brief Spark streak, white, its head on the right. */
    sq::ecs::TextureId spark = sq::ecs::NO_TEXTURE;
    /** @brief Font, or `sq::engine::NO_FONT` (the backend's default font is used). */
    sq::engine::FontId font = sq::engine::NO_FONT;
};

/**
 * @brief Resolves `GameAssets` from the aliases declared by `rlss/scripts/Assets.wren` and adds it to the registry as
 * a resource. The `sq::assets::AssetManager` owns the textures and the font.
 */
class AssetsPlugin final : public sq::engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "AssetsPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "AssetsPlugin"; }

    /**
     * @brief Declares the dependencies on `sq::render::RenderPlugin` (textures and font need its window) and
     * `sq::assets::AssetScriptingPlugin` (the declarations) and `sq::assets::RenderAssetsPlugin` (the loaders).
     * @param builder Dependency builder.
     */
    void build(sq::engine::PluginBuilder& builder) override;

    /**
     * @brief Loads the global asset group and resolves every texture and the font.
     * @param engine Host engine.
     */
    void init(sq::engine::Engine& engine) override;
};

}  // namespace rlss::client
