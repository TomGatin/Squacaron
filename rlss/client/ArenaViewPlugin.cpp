#include "ArenaViewPlugin.hpp"

#include "Arena.hpp"
#include "AssetsPlugin.hpp"
#include "Shapes.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/time/Time.hpp"
#include "render/api/components/CameraComponent.hpp"
#include "render/api/components/Renderable.hpp"

namespace rlss::client {

namespace {

// Layout of the pictures, as drawn by rlss/assets/generate_textures.py (keep both in sync).
/** @brief Pixels per uu of the arena picture (`ARENA_SCALE`). */
constexpr float ARENA_SCALE = 1.25F;
/** @brief The arena picture extends this far past the outer walls (`ARENA_MARGIN`), in uu. */
constexpr float ARENA_MARGIN = 120.0F;
/** @brief Side of a full arena tile, in pixels (`TILE`). */
constexpr float TILE = 1024.0F;
/** @brief Pixels per uu of the stadium picture (`STADIUM_SCALE`). */
constexpr float STADIUM_SCALE = 0.6F;

/**
 * @brief Share of the camera motion the stadium follows: 0 would pin it to the arena, 1 to the screen. It looks far
 * behind the glass, moving at 40% of the arena's speed on screen.
 */
constexpr float STADIUM_FOLLOW = 0.6F;
/** @brief The stadium sits a little lower than the arena center: its stands rise above the floor. */
constexpr float STADIUM_DROP = 60.0F;
/** @brief Fills whatever the stadium does not cover, far away. */
constexpr float BACKDROP_SIZE = 12000.0F;
constexpr sq::ecs::Rgba BACKDROP{.r = 8, .g = 10, .b = 22, .a = 255};

constexpr int LAYER_BACKDROP = 0;
constexpr int LAYER_STADIUM = 1;
constexpr int LAYER_ARENA = 2;

constexpr float CENTER_X = (arena::LEFT + arena::RIGHT) / 2.0F;
constexpr float CENTER_Y = ((arena::TOP + arena::BOTTOM) / 2.0F) + STADIUM_DROP;

sq::ecs::Renderable textured(const sq::ecs::TextureId texture, const int layer, const sq::ecs::RenderOrigin origin) {
    sq::ecs::Renderable renderable;
    renderable.textureId = texture;
    renderable.layer = layer;
    renderable.origin = origin;
    return renderable;
}

}  // namespace

void ArenaViewPlugin::build(sq::engine::PluginBuilder& builder) { builder.require<AssetsPlugin>(); }

void ArenaViewPlugin::init(sq::engine::Engine& engine) {
    sq::ecs::Registry& registry = engine.registry();
    const GameAssets& assets = registry.getResource<GameAssets>();
    const float half = BACKDROP_SIZE / 2.0F;
    spawnRect(registry,
              {.left = CENTER_X - half, .top = CENTER_Y - half, .right = CENTER_X + half, .bottom = CENTER_Y + half},
              BACKDROP, LAYER_BACKDROP);

    if (assets.stadium != sq::ecs::NO_TEXTURE) {
        _stadium = registry.spawnEntity();
        registry.addComponent(_stadium, sq::ecs::Transform{
                                            .x = CENTER_X,
                                            .y = CENTER_Y,
                                            .scaleX = 1.0F / STADIUM_SCALE,
                                            .scaleY = 1.0F / STADIUM_SCALE,
                                        });
        registry.addComponent(_stadium, textured(assets.stadium, LAYER_STADIUM, {.x = 0.5F, .y = 0.5F}));
    }

    // Tiles are anchored on their top-left corner, in rows and columns of TILE pixels.
    const float left = arena::LEFT - arena::GOAL_DEPTH - ARENA_WALL - ARENA_MARGIN;
    const float top = arena::TOP - ARENA_WALL - ARENA_MARGIN;
    const float tileSize = TILE / ARENA_SCALE;
    for (const ArenaTile& tile : assets.arena) {
        if (tile.texture == sq::ecs::NO_TEXTURE) {
            continue;
        }
        const sq::ecs::Entity entity = registry.spawnEntity();
        registry.addComponent(entity, sq::ecs::Transform{
                                          .x = left + (static_cast<float>(tile.column) * tileSize),
                                          .y = top + (static_cast<float>(tile.row) * tileSize),
                                          .scaleX = 1.0F / ARENA_SCALE,
                                          .scaleY = 1.0F / ARENA_SCALE,
                                      });
        registry.addComponent(entity, textured(tile.texture, LAYER_ARENA, {.x = 0.0F, .y = 0.0F}));
    }
}

void ArenaViewPlugin::lateUpdate(sq::engine::Engine& engine, const sq::time::Time& /*time*/) {
    sq::ecs::Registry& registry = engine.registry();
    auto& transforms = registry.getComponents<sq::ecs::Transform>();
    if (_stadium == 0 || !transforms.contains(_stadium)) {
        return;
    }
    const sq::ecs::CameraComponent* camera = nullptr;
    registry.runSystem<sq::ecs::CameraComponent>(
        [&camera](sq::ecs::Entity /*entity*/, const sq::ecs::CameraComponent& component) {
            if (camera == nullptr && component.active) {
                camera = &component;
            }
        });
    if (camera == nullptr) {
        return;
    }
    sq::ecs::Transform& stadium = transforms.get(_stadium);
    stadium.x = CENTER_X + ((camera->targetX - CENTER_X) * STADIUM_FOLLOW);
    stadium.y = CENTER_Y + ((camera->targetY - CENTER_Y) * STADIUM_FOLLOW);
}

}  // namespace rlss::client
