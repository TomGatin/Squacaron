#include "AssetsPlugin.hpp"

#include <string>
#include <string_view>
#include <utility>

#include "assets/AssetKinds.hpp"
#include "assets/AssetManager.hpp"
#include "assets/render/RenderAssetsPlugin.hpp"
#include "assets/scripting/AssetScriptingPlugin.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Registry.hpp"
#include "log/Logger.hpp"
#include "render/RenderPlugin.hpp"
#include "render/api/IScreenDrawService.hpp"

namespace rlss::client {

namespace {

constexpr const char* LOG_TAG = "assets";
/** @brief More tiles than the arena will ever need: the lookup stops at the first missing one. */
constexpr int MAX_TILES = 16;

std::string tileAlias(const int row, const int column) {
    return "arena_" + std::to_string(row) + "_" + std::to_string(column);
}

}  // namespace

void AssetsPlugin::build(sq::engine::PluginBuilder& builder) {
    builder.require<sq::render::RenderPlugin>();
    builder.require<sq::assets::AssetScriptingPlugin>();
    // The texture and font loaders: without them the global group would only load once they are installed.
    builder.require<sq::assets::RenderAssetsPlugin>();
}

void AssetsPlugin::init(sq::engine::Engine& engine) {
    auto& manager = engine.registry().getResource<sq::assets::AssetManager>();
    manager.loadGroup(sq::assets::AssetManager::GLOBAL_GROUP);
    // A missing file is drawn as the checkerboard (the game stays playable), but always reported. A file absent
    // from its directory is not even declared by the preload: it gets the checkerboard too.
    const auto texture = [&manager](const std::string_view alias) {
        const sq::assets::AssetStatus status = manager.status<sq::assets::Texture>(alias);
        if (status != sq::assets::AssetStatus::Loaded) {
            sq::engine::log::warning(LOG_TAG, "texture '", alias,
                                     "' not loaded (are rlss/scripts/Assets.wren and assets/ next to the executable?)");
        }
        return status == sq::assets::AssetStatus::Unknown ? manager.fallback<sq::assets::Texture>()
                                                          : manager.get<sq::assets::Texture>(alias);
    };
    const auto isDeclared = [&manager](const std::string& alias) {
        return manager.status<sq::assets::Texture>(alias) != sq::assets::AssetStatus::Unknown;
    };

    GameAssets loaded;
    loaded.stadium = texture("stadium");
    for (int row = 0; row < MAX_TILES && isDeclared(tileAlias(row, 0)); ++row) {
        for (int column = 0; column < MAX_TILES && isDeclared(tileAlias(row, column)); ++column) {
            loaded.arena.push_back({.texture = texture(tileAlias(row, column)), .row = row, .column = column});
        }
    }
    if (loaded.arena.empty()) {
        sq::engine::log::warning(LOG_TAG, "no arena texture");
    }
    loaded.carBody = texture("car_body");
    loaded.carDetails = texture("car_details");
    loaded.wheel = texture("wheel");
    loaded.flame = texture("boost_flame");
    loaded.glow = texture("glow");
    loaded.puff = texture("puff");
    loaded.ball = texture("ball");
    loaded.ballShade = texture("ball_shade");
    loaded.hudDisc = texture("hud_disc");
    loaded.hudTick = texture("hud_tick");
    loaded.hudSlantLeft = texture("hud_slant_left");
    loaded.hudSlantRight = texture("hud_slant_right");
    loaded.hudBand = texture("hud_band");
    loaded.explosionBlue = texture("goal_explosion_blue");
    loaded.explosionOrange = texture("goal_explosion_orange");
    loaded.shockwave = texture("shockwave");
    loaded.spark = texture("spark");
    loaded.font = manager.get<sq::assets::Font>(FONT_ALIAS);
    if (loaded.font == sq::engine::NO_FONT) {
        sq::engine::log::warning(LOG_TAG, "no font '", FONT_ALIAS, "', using the default font");
    }
    engine.registry().addResource(std::move(loaded));
}

}  // namespace rlss::client
