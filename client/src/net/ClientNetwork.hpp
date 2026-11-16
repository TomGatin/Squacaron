#pragma once

#include "core/Engine.hpp"

namespace client {

/**
 * @brief Adds everything the client needs to play online; nothing connects until a `ConnectRequest` is published.
 *
 * Adds `sq::net::NetworkPlugin`, `sq::net::ClientReplicationPlugin`, `RTypeNetworkPlugin` (with a factory making a
 * fresh asio transport for each connection) and `PlayerControlPlugin`.
 *
 * @param engine Engine of the client.
 */
void addNetworkPlugins(sq::engine::Engine& engine);

}  // namespace client
