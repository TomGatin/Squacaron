#include "SessionPlugin.hpp"

#include <memory>
#include <utility>

#include "ServerLink.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "log/Logger.hpp"
#include "net/INetworkClient.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"

namespace rlss::client {

namespace {

constexpr const char* LOG_TAG = "client";

}  // namespace

SessionPlugin::SessionPlugin(std::shared_ptr<sq::net::INetworkClient> transport) : _transport(std::move(transport)) {}

void SessionPlugin::build(sq::engine::PluginBuilder& builder) { builder.require<sq::net::NetworkPlugin>(); }

void SessionPlugin::init(sq::engine::Engine& engine) {
    auto& events = engine.eventBus();
    _disconnected =
        events.subscribe<sq::net::PeerDisconnected>([this, &engine](const sq::net::PeerDisconnected& /*event*/) {
            sq::engine::log::error(LOG_TAG, "disconnected: ", describe(_transport->disconnectReason()));
            _lost = true;
            engine.stop();
        });
}

void SessionPlugin::shutdown(sq::engine::Engine& engine) {
    auto& events = engine.eventBus();
    if (_disconnected) {
        events.unsubscribe<sq::net::PeerDisconnected>(*_disconnected);
        _disconnected.reset();
    }
}

}  // namespace rlss::client
