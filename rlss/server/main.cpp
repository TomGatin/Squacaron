/**
 * @file main.cpp
 * @brief RLSS server entry point: `rlss_server [port]`.
 */

#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <span>

#include "CommandLine.hpp"
#include "MatchServer.hpp"
#include "core/ContentRoot.hpp"
#include "core/FixedRateLoop.hpp"
#include "log/Logger.hpp"
#include "net/INetworkServer.hpp"
#include "net/NetworkFactory.hpp"
#include "protocol/Protocol.hpp"

namespace {

constexpr int EXIT_ERROR = 84;
constexpr const char* LOG_TAG = "server";
/** @brief Polls of the transport per second: about one per millisecond, without spinning a core. */
constexpr double MENU_LOOP_RATE = 1000.0;

}  // namespace

int main(int ac, char** av) {
    const std::span args(av, static_cast<std::size_t>(ac));
    const auto values = args.subspan(1);
    std::optional<std::uint16_t> port = rlss::protocol::DEFAULT_PORT;
    if (values.size() > 1) {
        port.reset();
    } else if (!values.empty()) {
        port = rlss::parsePort(values.front());
    }
    if (!port) {
        std::cerr << "usage: " << args.front() << " [port]\n";
        return EXIT_ERROR;
    }

    try {
        const std::shared_ptr<sq::net::INetworkServer> transport =
            sq::net::createServer({.protocolId = rlss::protocol::PROTOCOL_ID});
        if (!transport->start(*port)) {
            sq::engine::log::error(LOG_TAG, "cannot listen on port ", *port);
            return EXIT_ERROR;
        }
        sq::engine::log::info(LOG_TAG, rlss::protocol::GAME_TITLE, " server listening on port ", *port);

        {
            // The menu runs here; each match runs on its own thread, with its own engine.
            // The scripts are read next to the executable, so the server runs from any working directory.
            rlss::server::MatchServer server(transport, true, sq::engine::contentRoot());
            sq::engine::FixedRateLoop loop(MENU_LOOP_RATE);
            loop.stopOnSignals(true);
            loop.run([&server] { server.update(); });
            sq::engine::log::info(LOG_TAG, "shutting down");
        }
        transport->stop();
    } catch (const std::exception& error) {
        std::cerr << "server error: " << error.what() << '\n';
        return EXIT_ERROR;
    }
    return 0;
}
