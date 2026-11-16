#include "net/ClientNetwork.hpp"

#include <memory>

#include "core/Engine.hpp"
#include "game/ReplicationCodec.hpp"
#include "net/ClientConnection.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/NetworkFactory.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/PlayerControlPlugin.hpp"
#include "net/RTypeNetworkPlugin.hpp"
#include "net/metrics/NetworkMetricsPlugin.hpp"
#include "protocol/Messages.hpp"

namespace client {

void addNetworkPlugins(sq::engine::Engine& engine) {
    const auto connection = std::make_shared<sq::net::ClientConnection>(nullptr);
    const RTypeNetworkPlugin::TransportFactory factory = [] {
        return sq::net::createClient({.protocolId = rtype::protocol::TRANSPORT_PROTOCOL_ID});
    };

    engine.addPlugin<sq::net::NetworkPlugin>(connection, connection)
        .addPlugin<sq::net::ClientReplicationPlugin>(std::make_shared<rtype::game::ReplicationCodec>())
        .addPlugin<RTypeNetworkPlugin>(connection, factory)
        .addPlugin<PlayerControlPlugin>()
        .addPlugin<sq::net::NetworkMetricsPlugin>([connection] { return connection->stats(); });
}

}  // namespace client
