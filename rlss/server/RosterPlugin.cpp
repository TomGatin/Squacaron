#include "RosterPlugin.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "Arena.hpp"
#include "CarSimulationPlugin.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "game/Components.hpp"
#include "input/InputState.hpp"
#include "log/Logger.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkService.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/ReplicationSystems.hpp"
#include "net/components/InputBuffer.hpp"
#include "net/components/NetworkOwner.hpp"
#include "protocol/Protocol.hpp"
#include "scripting/ScriptComponent.hpp"
#include "scripting/ScriptPlugin.hpp"

namespace rlss::server {

namespace {

constexpr const char* LOG_TAG = "match";
/** @brief Name of a player who has not sent one, followed by its slot number. */
constexpr const char* DEFAULT_NAME = "JOUEUR ";
}  // namespace

void RosterPlugin::build(sq::engine::PluginBuilder& builder) {
    builder.require<sq::net::NetworkPlugin>();
    builder.require<CarSimulationPlugin>();
}

void RosterPlugin::init(sq::engine::Engine& engine) {
    auto& events = engine.eventBus();
    _connected = events.subscribe<sq::net::PeerConnected>([this, &engine](const sq::net::PeerConnected& event) {
        if (join(engine, event.peer)) {
            sq::engine::log::info(LOG_TAG, "player ", event.peer, " joined (", _players.size(), " playing)");
        }
    });
    _disconnected =
        events.subscribe<sq::net::PeerDisconnected>([this, &engine](const sq::net::PeerDisconnected& event) {
            if (leave(engine, event.peer)) {
                sq::engine::log::info(LOG_TAG, "player ", event.peer, " left (", _players.size(), " playing)");
            }
        });
    _packets = events.subscribe<sq::net::PacketReceived>([this, &engine](const sq::net::PacketReceived& packet) {
        const auto player = _players.find(packet.peer);
        if (player == _players.end()) {
            return;
        }
        const auto message = protocol::decode(packet.data);  // untrusted: a malformed name is dropped
        const auto* name = message ? std::get_if<protocol::SetName>(&*message) : nullptr;
        if (name != nullptr && name->name != player->second.name) {
            player->second.name = name->name;
            broadcastNames(engine);
        }
    });
}

void RosterPlugin::broadcastNames(sq::engine::Engine& engine) const {
    protocol::PlayerNames names;
    for (const auto& [peer, player] : _players) {
        names.players.push_back(protocol::PlayerName{.carId = player.carId, .name = player.name});
    }
    engine.service<sq::net::INetworkService>().broadcast(protocol::encode(names), sq::net::Channel::Reliable);
}

void RosterPlugin::shutdown(sq::engine::Engine& engine) {
    auto& events = engine.eventBus();
    if (_connected) {
        events.unsubscribe<sq::net::PeerConnected>(*_connected);
        _connected.reset();
    }
    if (_disconnected) {
        events.unsubscribe<sq::net::PeerDisconnected>(*_disconnected);
        _disconnected.reset();
    }
    if (_packets) {
        events.unsubscribe<sq::net::PacketReceived>(*_packets);
        _packets.reset();
    }
    _players.clear();
    _slots = {};
}

std::uint8_t RosterPlugin::playersIn(const protocol::Team team) const noexcept {
    std::uint8_t count = 0;
    for (auto slot = static_cast<std::size_t>(team); slot < _slots.size(); slot += 2) {
        count += _slots.at(slot) ? 1 : 0;
    }
    return count;
}

std::optional<std::size_t> RosterPlugin::freeSlot() const {
    const protocol::Team team = playersIn(protocol::Team::Orange) < playersIn(protocol::Team::Blue)
                                    ? protocol::Team::Orange
                                    : protocol::Team::Blue;
    for (auto slot = static_cast<std::size_t>(team); slot < _slots.size(); slot += 2) {
        if (!_slots.at(slot)) {
            return slot;
        }
    }
    return std::nullopt;
}

bool RosterPlugin::join(sq::engine::Engine& engine, const sq::net::PeerId peer) {
    if (_players.contains(peer)) {
        return false;
    }
    const auto slot = freeSlot();
    if (!slot) {
        // The MatchServer never lets a fifth player in; refuse anyway rather than overflow the teams.
        sq::engine::log::warning(LOG_TAG, "match full, player ", peer, " refused");
        return false;
    }
    _slots.at(*slot) = true;

    sq::ecs::Registry& registry = engine.registry();
    const sq::ecs::Entity car = registry.spawnEntity();
    // The `Car` script moves it to its kickoff spot (`placeAtKickoff`): Kickoff.wren owns the spots.
    registry.addComponent(car, sq::ecs::Transform{.x = (arena::LEFT + arena::RIGHT) / 2.0F, .y = arena::BOTTOM / 2.0F});
    sq::input::InputState controls;
    controls.playerIndex = static_cast<int>(*slot);
    registry.addComponent(car, std::move(controls));
    registry.addComponent(car, sq::ecs::InputBuffer{});
    registry.addComponent(car, sq::ecs::NetworkOwner{.client = peer});
    registry.addComponent(car, game::CarLook{.color = static_cast<std::uint8_t>(*slot % PLAYER_COLORS.size())});
    registry.addComponent(car, game::CarStatus{.boost = 0.0F, .boosting = false, .placeAtKickoff = true});
    registry.addComponent(car, game::CarCues{});
    // Stepped by the `Match` script, not by its own update.
    registry.addComponent(car, sq::scripting::ScriptComponent{
                                   .moduleName = std::string(game::CAR_SCRIPT_MODULE),
                                   .className = "Car",
                               });
    const std::uint32_t carId = sq::net::replicate(registry, car, game::KIND_CAR);
    // The client needs to know which car is its own (its camera follows it) and its team (who won).
    engine.service<sq::net::INetworkService>().send(
        peer, protocol::encode(protocol::Welcome{.carId = carId, .team = static_cast<protocol::Team>(*slot % 2)}),
        sq::net::Channel::Reliable);
    _players.emplace(peer, Player{
                               .slot = *slot,
                               .car = car,
                               .carId = carId,
                               .name = DEFAULT_NAME + std::to_string(*slot + 1),
                           });
    broadcastNames(engine);
    return true;
}

bool RosterPlugin::leave(sq::engine::Engine& engine, const sq::net::PeerId peer) {
    const auto it = _players.find(peer);
    if (it == _players.end()) {
        return false;
    }
    // Through the script system: it drops the script instance before the entity is killed.
    engine.getPlugin<sq::scripting::ScriptPlugin>().scriptSystem().queueDestroyEntity(it->second.car);
    _slots.at(it->second.slot) = false;
    _players.erase(it);
    broadcastNames(engine);
    return true;
}

}  // namespace rlss::server
