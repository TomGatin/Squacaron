#include "MatchRoom.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>

#include "CarSimulationPlugin.hpp"
#include "RosterPlugin.hpp"
#include "game/ReplicationCodec.hpp"
#include "log/Logger.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkServer.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/PeerGroupConnection.hpp"
#include "net/ServerReplicationPlugin.hpp"
#include "protocol/Protocol.hpp"
#include "scripting/ScriptPlugin.hpp"

namespace rlss::server {

namespace {

constexpr const char* LOG_TAG = "match";

}  // namespace

MatchRoom::MatchRoom(sq::net::INetworkServer& transport, const std::uint16_t id,
                     const std::filesystem::path& scriptRoot)
    : _id(id), _connection(std::make_shared<sq::net::PeerGroupConnection>(transport)) {
    _engine.addPlugin<sq::net::NetworkPlugin>(_connection, _connection)
        .addPlugin<sq::net::ServerReplicationPlugin>(std::make_shared<game::ReplicationCodec>(id))
        .addPlugin<sq::scripting::ScriptPlugin>(sq::scripting::ScriptTick::EveryFrame, scriptRoot)
        .addPlugin<CarSimulationPlugin>()
        .addPlugin<RosterPlugin>();
    _engine.init();
}

MatchRoom::~MatchRoom() {
    _loop.stop();
    if (_thread.joinable()) {
        _thread.join();
    }
    _engine.shutdown();
}

bool MatchRoom::addPlayer(const sq::net::PeerId peer) {
    if (isFull() || std::ranges::contains(_players, peer)) {
        return false;
    }
    _players.push_back(peer);
    _connection->addPeer(peer);
    _connection->push({.type = sq::net::EventType::Connected, .peer = peer, .data = {}, .channel = {}});
    return true;
}

bool MatchRoom::removePlayer(const sq::net::PeerId peer) {
    const auto it = std::ranges::find(_players, peer);
    if (it == _players.end()) {
        return false;
    }
    _players.erase(it);
    // Pushed while the peer is still in the group (events of other peers are dropped), then the group forgets it: the
    // match can no longer reach it, but still gets the event.
    _connection->push({.type = sq::net::EventType::Disconnected, .peer = peer, .data = {}, .channel = {}});
    _connection->removePeer(peer);
    return true;
}

void MatchRoom::deliver(const sq::net::PeerId peer, const sq::net::Bytes& data, const sq::net::Channel channel) {
    _connection->push({.type = sq::net::EventType::Data, .peer = peer, .data = data, .channel = channel});
}

void MatchRoom::advance() {
    _engine.advance();
    MatchSummary summary;
    if (const auto status = _engine.getPlugin<CarSimulationPlugin>().currentStatus(_engine)) {
        summary.status = *status;
    }
    const auto& roster = _engine.getPlugin<RosterPlugin>();
    summary.bluePlayers = roster.playersIn(protocol::Team::Blue);
    summary.orangePlayers = roster.playersIn(protocol::Team::Orange);
    const std::scoped_lock lock(_summaryMutex);
    _summary = summary;
}

void MatchRoom::runInBackground() {
    _thread = std::thread([this] {
        try {
            _loop.run([this] { advance(); });
        } catch (const std::exception& error) {
            sq::engine::log::error(LOG_TAG, "match ", _id, " stopped by an error in its simulation: ", error.what());
            fail();
        }
    });
}

void MatchRoom::fail() {
    protocol::MatchStatus status;
    {
        const std::scoped_lock lock(_summaryMutex);
        _summary.status.phase = protocol::MatchPhase::Ended;
        status = _summary.status;
    }
    // The engine no longer runs: tell the players directly, their end screen takes them back to the menu.
    _connection->broadcast(protocol::encode(status), sq::net::Channel::Reliable);
}

MatchSummary MatchRoom::summary() const {
    const std::scoped_lock lock(_summaryMutex);
    return _summary;
}

}  // namespace rlss::server
