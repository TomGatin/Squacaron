#include "ServerLink.hpp"

#include <chrono>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "net/INetworkClient.hpp"
#include "net/INetworkManager.hpp"
#include "protocol/Protocol.hpp"

namespace rlss::client {

namespace {

constexpr std::chrono::milliseconds POLL_INTERVAL{10};

}  // namespace

void ServerLink::send(const sq::net::PeerId peer, const sq::net::Bytes& data, const sq::net::Channel channel) {
    if (peer == sq::net::SERVER_PEER) {
        _client->send(data, channel);
    }
}

void ServerLink::broadcast(const sq::net::Bytes& data, const sq::net::Channel channel) { _client->send(data, channel); }

void ServerLink::kick(const sq::net::PeerId /*peer*/) { _client->stop(); }

std::vector<sq::net::NetworkEvent> ServerLink::poll() {
    std::vector<sq::net::NetworkEvent> events = std::exchange(_pending, {});
    std::vector<sq::net::NetworkEvent> received = _client->poll();
    events.insert(events.end(), std::make_move_iterator(received.begin()), std::make_move_iterator(received.end()));
    std::erase_if(events, [this](const sq::net::NetworkEvent& event) {
        if (event.type != sq::net::EventType::Data) {
            return false;
        }
        const auto match = protocol::snapshotMatch(event.data);
        return match && match != _match;
    });
    return events;
}

std::optional<std::vector<sq::net::NetworkEvent>> connectToServer(sq::net::INetworkClient& client,
                                                                  const std::string& host, const std::uint16_t port,
                                                                  const std::chrono::milliseconds timeout) {
    if (!client.connect(host, port)) {
        return std::nullopt;
    }
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::vector<sq::net::NetworkEvent> events;
    bool connected = false;
    while (std::chrono::steady_clock::now() < deadline) {
        for (sq::net::NetworkEvent& event : client.poll()) {
            if (event.type == sq::net::EventType::Disconnected && !connected) {
                return std::nullopt;
            }
            connected = connected || event.type == sq::net::EventType::Connected;
            if (connected) {
                events.push_back(std::move(event));
            }
        }
        if (connected) {
            return events;
        }
        std::this_thread::sleep_for(POLL_INTERVAL);
    }
    client.stop();
    return std::nullopt;
}

const char* describe(const sq::net::DisconnectReason reason) noexcept {
    switch (reason) {
        case sq::net::DisconnectReason::ProtocolMismatch:
            return "the server runs another game or version";
        case sq::net::DisconnectReason::ServerFull:
            return "the server is full";
        case sq::net::DisconnectReason::Unreachable:
            return "the server cannot be reached";
        case sq::net::DisconnectReason::ConnectionLost:
            break;
    }
    return "connection to the server lost";
}

}  // namespace rlss::client
