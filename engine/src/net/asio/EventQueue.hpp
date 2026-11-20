#pragma once

#include <cstddef>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "net/INetworkManager.hpp"
#include "net/asio/TransportCounters.hpp"

namespace ecs::net::detail {

/**
 * @brief Hands events from the network thread to the game thread.
 *
 * Bounded per peer: once a peer has `maxDataPerPeer` `Data` events waiting, its next ones are dropped and counted in
 * `TransportCounters::onEventDropped()`, so a flood on a slow game thread cannot grow memory without limit.
 * `Connected` and `Disconnected` are never dropped: the game must always know who is connected.
 */
class EventQueue {
  public:
    /**
     * @brief Creates an empty queue.
     * @param maxDataPerPeer `Data` events kept per peer between two `popAll()`; must be at least 1.
     * @param counters Where the drops are counted; must outlive the queue.
     */
    EventQueue(std::size_t maxDataPerPeer, TransportCounters& counters)
        : _maxDataPerPeer(maxDataPerPeer), _counters(&counters) {}

    /**
     * @brief Adds an event (network thread).
     * @param event Event to queue.
     * @return False if it was dropped because its peer is over the cap.
     */
    bool push(NetworkEvent event);
    /** @brief Takes every queued event, oldest first (game thread). */
    [[nodiscard]] std::vector<NetworkEvent> popAll();
    /** @brief Number of events waiting (any thread). */
    [[nodiscard]] std::size_t size();

  private:
    /** @brief Cap on the `Data` events of one peer. */
    std::size_t _maxDataPerPeer;
    /** @brief Drop counter of the transport. */
    TransportCounters* _counters;
    /** @brief Guards `_events` and `_dataPerPeer`. */
    std::mutex _mutex;
    /** @brief Events not polled yet. */
    std::vector<NetworkEvent> _events;
    /** @brief `Data` events waiting, per peer. */
    std::unordered_map<PeerId, std::size_t> _dataPerPeer;
};

}  // namespace ecs::net::detail
