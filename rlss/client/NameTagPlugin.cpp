#include "NameTagPlugin.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "Arena.hpp"
#include "AssetsPlugin.hpp"
#include "CameraPlugin.hpp"
#include "ClientEvents.hpp"
#include "UiCanvas.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/time/Time.hpp"
#include "game/Components.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/NetworkEntityMap.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"
#include "protocol/Protocol.hpp"
#include "render/api/IScreenDrawService.hpp"
#include "render/api/components/CameraComponent.hpp"
#include "render/api/components/Renderable.hpp"
#include "scripting/ScriptPlugin.hpp"
#include "tools/console/ConsolePlugin.hpp"
#include "tools/cvars/CVarManager.hpp"

namespace rlss::client {

namespace {

constexpr sq::ecs::Rgba BACKGROUND_COLOR{.r = 6, .g = 10, .b = 22, .a = 165};
/** @brief Height of the capital letters, in pixels: the label keeps its size whatever the zoom. */
constexpr float CAP_HEIGHT = 13.0F;
/** @brief Space around the letters, in pixels. */
constexpr float PADDING = 6.0F;
/** @brief Thickness of the line in the player color under the name, in pixels. */
constexpr float ACCENT = 3.0F;
/** @brief The bottom of the label is this far above the car center, in world units: above the boost gauge. */
constexpr float HEIGHT_ABOVE_CAR = 74.0F;
/** @brief The player color is mixed with this much white, to read well on the dark label. */
constexpr float WHITE_MIX = 0.45F;

sq::ecs::Rgba lighter(const Rgb color) {
    const auto mixed = [](const std::uint8_t channel) {
        const auto value = static_cast<float>(channel);
        return static_cast<std::uint8_t>(value + ((255.0F - value) * WHITE_MIX));
    };
    return sq::ecs::Rgba{.r = mixed(color.r), .g = mixed(color.g), .b = mixed(color.b), .a = 255};
}

}  // namespace

void NameTagPlugin::build(sq::engine::PluginBuilder& builder) {
    builder.require<sq::net::NetworkPlugin>();
    builder.require<sq::net::ClientReplicationPlugin>();
    // The camera script runs in the ScriptPlugin update: the labels are placed after it, with the final camera.
    builder.require<sq::scripting::ScriptPlugin>();
    builder.require<CameraPlugin>();
    builder.require<AssetsPlugin>();
    builder.optionally<sq::tools::ConsolePlugin>();
}

void NameTagPlugin::init(sq::engine::Engine& engine) {
    if (auto cvars = engine.mediator().getService<sq::tools::CVarManager>(); cvars) {
        _visible = cvars->get().add("cl_nametags", true, "Shows the player names above the cars");
    }
    _typeface =
        Typeface(engine.service<sq::engine::IScreenDrawService>(), engine.registry().getResource<GameAssets>().font);
    auto& events = engine.eventBus();
    _packets = events.subscribe<sq::net::PacketReceived>([this](const sq::net::PacketReceived& packet) {
        const auto message = protocol::decode(packet.data);
        const auto* names = message ? std::get_if<protocol::PlayerNames>(&*message) : nullptr;
        if (names == nullptr || !_inMatch) {
            return;
        }
        _names.clear();
        for (const protocol::PlayerName& player : names->players) {
            _names.insert_or_assign(player.carId, player.name);
        }
    });
    _joined = events.subscribe<MatchJoined>([this](const MatchJoined& /*event*/) { _inMatch = true; });
    _left = events.subscribe<MatchLeft>([this, &engine](const MatchLeft& /*event*/) {
        _inMatch = false;
        _names.clear();
        clearTags(engine);
    });
}

void NameTagPlugin::update(sq::engine::Engine& engine, const sq::time::Time& /*time*/) {
    sq::ecs::Registry& registry = engine.registry();
    const auto& map = registry.getResource<sq::net::NetworkEntityMap>();
    auto& transforms = registry.getComponents<sq::ecs::Transform>();

    // Drop the labels whose name or car is gone, or whose name changed.
    std::vector<std::uint32_t> stale;
    for (const auto& [carId, tag] : _tags) {
        const auto name = _names.find(carId);
        const auto car = map.entityOf(carId);
        if (name == _names.end() || name->second != tag.name || !car || *car != tag.car || !transforms.contains(*car)) {
            stale.push_back(carId);
        }
    }
    for (const std::uint32_t carId : stale) {
        _tags.erase(carId);
    }

    // Build the missing ones, once their car is there.
    const auto& looks = registry.getComponents<game::CarLook>();
    for (const auto& [carId, name] : _names) {
        const auto car = map.entityOf(carId);
        if (_tags.contains(carId) || !car || !transforms.contains(*car)) {
            continue;
        }
        Tag& tag = _tags[carId];
        tag.car = *car;
        tag.name = name;
        // Around the anchor point (0, 0): the middle of the bottom edge of the label.
        const Rgb color = playerColor(looks.contains(*car) ? looks.get(*car).color : 0);
        const float textWidth = _typeface.width(name, CAP_HEIGHT);
        const ScreenRect plate{
            .x = -(textWidth / 2.0F) - PADDING,
            .y = -CAP_HEIGHT - (2.0F * PADDING) - ACCENT,
            .width = textWidth + (2.0F * PADDING),
            .height = CAP_HEIGHT + (2.0F * PADDING),
        };
        tag.canvas.rect(plate, BACKGROUND_COLOR);
        tag.canvas.rect({.x = plate.x, .y = plate.y + plate.height, .width = plate.width, .height = ACCENT},
                        sq::ecs::Rgba{.r = color.r, .g = color.g, .b = color.b, .a = 255});
        tag.canvas.text(name, 0.0F, plate.y + PADDING, CAP_HEIGHT, lighter(color));
    }

    // Follow the cars: the labels are laid out in pixels, shifted to the point above each car on screen.
    if (_visible && !_visible.get()) {
        return;
    }
    const sq::ecs::Entity cameraEntity = engine.getPlugin<CameraPlugin>().camera();
    const auto& cameras = registry.getComponents<sq::ecs::CameraComponent>();
    if (!cameras.contains(cameraEntity)) {
        return;
    }
    const sq::ecs::CameraComponent camera = cameras.get(cameraEntity);
    const float zoom = camera.zoom > 0.0F ? camera.zoom : 1.0F;
    for (const auto& [carId, tag] : _tags) {
        const sq::ecs::Transform& car = transforms.get(tag.car);
        const float screenX = camera.offsetX + ((car.x - camera.targetX) * zoom);
        const float screenY = camera.offsetY + ((car.y - HEIGHT_ABOVE_CAR - camera.targetY) * zoom);
        tag.canvas.draw(_typeface, screenX, screenY);
    }
}

void NameTagPlugin::clearTags(sq::engine::Engine& /*engine*/) { _tags.clear(); }

void NameTagPlugin::shutdown(sq::engine::Engine& engine) {
    auto& events = engine.eventBus();
    if (_packets) {
        events.unsubscribe<sq::net::PacketReceived>(*_packets);
        _packets.reset();
    }
    if (_joined) {
        events.unsubscribe<MatchJoined>(*_joined);
        _joined.reset();
    }
    if (_left) {
        events.unsubscribe<MatchLeft>(*_left);
        _left.reset();
    }
    clearTags(engine);
    _names.clear();
}

}  // namespace rlss::client
