#include "BallViewPlugin.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "Arena.hpp"
#include "AssetsPlugin.hpp"
#include "CarViewPlugin.hpp"
#include "Shapes.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Tag.hpp"
#include "ecs/components/Transform.hpp"
#include "game/Components.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/ReplicationEvents.hpp"
#include "render/api/components/Renderable.hpp"
#include "scripting/ScriptComponent.hpp"
#include "scripting/ScriptPlugin.hpp"

namespace rlss::client {

namespace {

/** @brief Same radius as the physics (`BallTuning.RADIUS`). */
constexpr float BALL_RADIUS = rlss::BALL_RADIUS;
/** @brief Width of ball.png and ball_shade.png, in pixels: they span the ball diameter. */
constexpr float BALL_TEXTURE_SIZE = 512.0F;
/** @brief Width of glow.png, in pixels. */
constexpr float GLOW_TEXTURE_SIZE = 256.0F;
/** @brief The glow after a colored shot: a soft disc this many times wider than the ball, behind it. */
constexpr float HALO_SCALE = 2.6F;
constexpr sq::ecs::Rgba WHITE{.r = 255, .g = 255, .b = 255, .a = 255};
constexpr int LAYER_HALO = 13;
constexpr int LAYER_BALL = 14;
constexpr int LAYER_SHADE = 15;
/** @brief Halo color of a shot. */
sq::ecs::Rgba shotColor(const game::Shot shot) {
    constexpr std::uint8_t ALPHA = 255;
    switch (shot) {
        case game::Shot::Red:
            return {.r = 255, .g = 60, .b = 60, .a = ALPHA};
        case game::Shot::Purple:
            return {.r = 180, .g = 90, .b = 255, .a = ALPHA};
        case game::Shot::Gold:
            return {.r = 255, .g = 200, .b = 40, .a = ALPHA};
        case game::Shot::None:
            break;
    }
    return {.r = 0, .g = 0, .b = 0, .a = 0};
}
sq::ecs::Entity spawnPiece(sq::ecs::Registry& registry, const game::BallPart& part, const sq::ecs::TextureId texture,
                           const float scale, const sq::ecs::Rgba color, const int layer) {
    const sq::ecs::Entity piece = registry.spawnEntity();
    registry.addComponent(piece, sq::ecs::Transform{.x = 0.0F, .y = 0.0F, .scaleX = scale, .scaleY = scale});
    sq::ecs::Renderable renderable = makeRenderable(color, layer);
    renderable.textureId = texture;
    registry.addComponent(piece, sq::ecs::Renderable{renderable});
    registry.addComponent(piece, sq::ecs::Tag{.name = std::string(game::BALL_PART_TAG)});
    registry.addComponent(piece, part);
    return piece;
}

}  // namespace

void BallViewPlugin::build(sq::engine::PluginBuilder& builder) {
    builder.require<AssetsPlugin>();
    builder.require<CarViewPlugin>();
    builder.require<sq::net::ClientReplicationPlugin>();
}

void BallViewPlugin::init(sq::engine::Engine& engine) {
    auto& events = engine.eventBus();
    _spawned = events.subscribe<sq::net::ReplicatedEntitySpawned>(
        [this, &engine](const sq::net::ReplicatedEntitySpawned& event) {
            if (event.kind != game::KIND_BALL) {
                return;
            }
            sq::ecs::Registry& registry = engine.registry();
            std::vector<sq::ecs::Entity>& pieces = _pieces[event.entity];
            const GameAssets& assets = registry.getResource<GameAssets>();
            const float ballScale = 2.0F * BALL_RADIUS / BALL_TEXTURE_SIZE;
            // One halo per shot color, hidden until the ball glows with it.
            const float haloScale = 2.0F * BALL_RADIUS * HALO_SCALE / GLOW_TEXTURE_SIZE;
            for (const game::Shot shot : {game::Shot::Red, game::Shot::Purple, game::Shot::Gold}) {
                pieces.push_back(spawnPiece(registry,
                                            game::BallPart{.parent = event.entity, .glow = static_cast<int>(shot)},
                                            assets.glow, haloScale, shotColor(shot), LAYER_HALO));
            }
            // The panels turn with the ball; the lighting stays put, so it keeps looking round and lit from above.
            pieces.push_back(spawnPiece(registry, game::BallPart{.parent = event.entity, .spins = true}, assets.ball,
                                        ballScale, WHITE, LAYER_BALL));
            pieces.push_back(spawnPiece(registry, game::BallPart{.parent = event.entity}, assets.ballShade, ballScale,
                                        WHITE, LAYER_SHADE));
            registry.addComponent(event.entity, sq::scripting::ScriptComponent{
                                                    .moduleName = std::string(game::BALL_VIEW_SCRIPT_MODULE),
                                                    .className = "BallView",
                                                    .hasUpdate = true,
                                                });
        });
    _despawned = events.subscribe<sq::net::ReplicatedEntityDespawned>(
        [this, &engine](const sq::net::ReplicatedEntityDespawned& event) {
            const auto it = _pieces.find(event.entity);
            if (it == _pieces.end()) {
                return;
            }
            for (const sq::ecs::Entity piece : it->second) {
                if (engine.registry().isAlive(piece)) {
                    engine.registry().killEntity(piece);
                }
            }
            _pieces.erase(it);
            engine.getPlugin<sq::scripting::ScriptPlugin>().scriptSystem().triggerDestroyHook(event.entity);
        });
}

void BallViewPlugin::shutdown(sq::engine::Engine& engine) {
    auto& events = engine.eventBus();
    if (_spawned) {
        events.unsubscribe<sq::net::ReplicatedEntitySpawned>(*_spawned);
        _spawned.reset();
    }
    if (_despawned) {
        events.unsubscribe<sq::net::ReplicatedEntityDespawned>(*_despawned);
        _despawned.reset();
    }
    _pieces.clear();
}

}  // namespace rlss::client
