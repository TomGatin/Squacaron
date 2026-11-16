#include "CarViewPlugin.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "Arena.hpp"
#include "AssetsPlugin.hpp"
#include "Shapes.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "game/Components.hpp"
#include "game/ScriptBindings.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/ReplicationEvents.hpp"
#include "render/api/components/Renderable.hpp"
#include "scripting/ScriptComponent.hpp"
#include "scripting/ScriptPlugin.hpp"

namespace rlss::client {

namespace {

constexpr sq::ecs::Rgba WHITE{.r = 255, .g = 255, .b = 255, .a = 255};
constexpr sq::ecs::Rgba HEADLIGHT_COLOR{.r = 255, .g = 236, .b = 170, .a = 170};
constexpr sq::ecs::Rgba GAUGE_BACK_COLOR{.r = 6, .g = 10, .b = 22, .a = 190};
constexpr sq::ecs::Rgba GAUGE_FILL_COLOR{.r = 255, .g = 176, .b = 32, .a = 255};
constexpr sq::ecs::Rgba FLASH_COLOR{.r = 255, .g = 255, .b = 255, .a = 210};
/** @brief The body texture is greyscale: the player color, slightly lightened, keeps it bright. */
constexpr float BODY_LIGHTEN = 0.18F;

constexpr int LAYER_FLAME = 9;
constexpr int LAYER_WHEELS = 10;
constexpr int LAYER_BODY = 11;
constexpr int LAYER_DETAILS = 12;
constexpr int LAYER_LIGHT = 13;
constexpr int LAYER_FLASH = 14;
constexpr int LAYER_GAUGE_BACK = 30;
constexpr int LAYER_GAUGE_FILL = 31;

sq::ecs::Rgba lighten(const Rgb color, const float amount) {
    const auto channel = [amount](const std::uint8_t value) {
        return static_cast<std::uint8_t>(static_cast<float>(value) + ((255.0F - static_cast<float>(value)) * amount));
    };
    return sq::ecs::Rgba{.r = channel(color.r), .g = channel(color.g), .b = channel(color.b), .a = 255};
}

/**
 * @brief A part is placed and sized by the script: it only needs its look and layer here. Hidden (scale 0) until the
 * script places it.
 */
sq::ecs::Entity spawnPart(sq::ecs::Registry& registry, const sq::ecs::Transform& car, const sq::ecs::TextureId texture,
                          const sq::ecs::Rgba color, const int layer) {
    const sq::ecs::Entity part = registry.spawnEntity();
    registry.addComponent(part, sq::ecs::Transform{.x = car.x, .y = car.y, .scaleX = 0.0F, .scaleY = 0.0F});
    sq::ecs::Renderable renderable = makeRenderable(color, layer);
    renderable.textureId = texture;
    registry.addComponent(part, sq::ecs::Renderable{renderable});
    return part;
}

void killPart(sq::ecs::Registry& registry, const sq::ecs::Entity part) {
    if (part != 0 && registry.isAlive(part)) {
        registry.killEntity(part);
    }
}

}  // namespace

void CarViewPlugin::build(sq::engine::PluginBuilder& builder) {
    builder.require<AssetsPlugin>();
    builder.require<sq::scripting::ScriptPlugin>();
    builder.require<sq::net::ClientReplicationPlugin>();
}

void CarViewPlugin::init(sq::engine::Engine& engine) {
    if (!game::installScriptBindings(engine.getPlugin<sq::scripting::ScriptPlugin>().scriptSystem())) {
        throw std::runtime_error("CarViewPlugin: Wren refused the RLSS component bindings");
    }
    auto& events = engine.eventBus();
    _spawned =
        events.subscribe<sq::net::ReplicatedEntitySpawned>([&engine](const sq::net::ReplicatedEntitySpawned& event) {
            sq::ecs::Registry& registry = engine.registry();
            if (event.kind != game::KIND_CAR || !registry.hasComponent<sq::ecs::Transform>(event.entity)) {
                return;
            }
            const sq::ecs::Transform car = registry.getComponents<sq::ecs::Transform>().get(event.entity);
            const auto& looks = registry.getComponents<game::CarLook>();
            const Rgb color = playerColor(looks.contains(event.entity) ? looks.get(event.entity).color : 0);
            const GameAssets& assets = registry.getResource<GameAssets>();
            registry.addComponent(
                event.entity,
                game::CarParts{
                    .body = spawnPart(registry, car, assets.carBody, lighten(color, BODY_LIGHTEN), LAYER_BODY),
                    .cabin = spawnPart(registry, car, assets.carDetails, WHITE, LAYER_DETAILS),
                    .rearWheel = spawnPart(registry, car, assets.wheel, WHITE, LAYER_WHEELS),
                    .frontWheel = spawnPart(registry, car, assets.wheel, WHITE, LAYER_WHEELS),
                    .headlight = spawnPart(registry, car, assets.glow, HEADLIGHT_COLOR, LAYER_LIGHT),
                    .flame = spawnPart(registry, car, assets.flame, WHITE, LAYER_FLAME),
                    .gaugeBack = spawnPart(registry, car, sq::ecs::NO_TEXTURE, GAUGE_BACK_COLOR, LAYER_GAUGE_BACK),
                    .gaugeFill = spawnPart(registry, car, sq::ecs::NO_TEXTURE, GAUGE_FILL_COLOR, LAYER_GAUGE_FILL),
                    .flash = spawnPart(registry, car, assets.carBody, FLASH_COLOR, LAYER_FLASH),
                });
            registry.addComponent(event.entity, sq::scripting::ScriptComponent{
                                                    .moduleName = std::string(game::CAR_VIEW_SCRIPT_MODULE),
                                                    .className = "CarView",
                                                    .hasUpdate = true,
                                                });
        });
    _despawned = events.subscribe<sq::net::ReplicatedEntityDespawned>(
        [&engine](const sq::net::ReplicatedEntityDespawned& event) {
            sq::ecs::Registry& registry = engine.registry();
            auto& allParts = registry.getComponents<game::CarParts>();
            if (!allParts.contains(event.entity)) {
                return;
            }
            const game::CarParts parts = allParts.get(event.entity);
            const std::array<sq::ecs::Entity, 9> pieces{
                parts.body,  parts.cabin,     parts.rearWheel, parts.frontWheel, parts.headlight,
                parts.flame, parts.gaugeBack, parts.gaugeFill, parts.flash,
            };
            for (const sq::ecs::Entity part : pieces) {
                killPart(registry, part);
            }
            // The engine kills the car right after this event: drop its script instance first.
            engine.getPlugin<sq::scripting::ScriptPlugin>().scriptSystem().triggerDestroyHook(event.entity);
        });
}

void CarViewPlugin::shutdown(sq::engine::Engine& engine) {
    auto& events = engine.eventBus();
    if (_spawned) {
        events.unsubscribe<sq::net::ReplicatedEntitySpawned>(*_spawned);
        _spawned.reset();
    }
    if (_despawned) {
        events.unsubscribe<sq::net::ReplicatedEntityDespawned>(*_despawned);
        _despawned.reset();
    }
}

}  // namespace rlss::client
