#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "INetworkEventSource.hpp"
#include "INetworkManager.hpp"
#include "INetworkServer.hpp"
#include "INetworkService.hpp"

namespace sq::net {

/**
 * @brief Adapts a server transport to the `NetworkPlugin` of one engine, among several sharing that transport.
 *
 * A transport has a single event queue for every peer, so the code that owns it polls it once and routes each event
 * with `push()`. In the other direction, the engine can only reach the peers of its group: `broadcast()` does not
 * leak to the peers of another group, and `send()` / `kick()` ignore them.
 */
class PeerGroupConnection final : public INetworkService, public INetworkEventSource {
  public:
    /** @brief Default of `maxQueuedMessages`, the same as `NetworkConfig::maxQueuedMessages`. */
    static constexpr std::size_t DEFAULT_MAX_QUEUED_MESSAGES = 4096;

    /**
     * @brief Creates an empty group.
     * @param server Transport, which must outlive this connection.
     * @param maxQueuedMessages `Data` events kept per peer until `poll()`; the next ones are dropped (at least 1).
     */
    explicit PeerGroupConnection(INetworkServer& server, std::size_t maxQueuedMessages = DEFAULT_MAX_QUEUED_MESSAGES)
        : _server(&server), _maxQueuedMessages(maxQueuedMessages > 0 ? maxQueuedMessages : 1) {}

    /**
     * @brief Adds a peer to the group.
     * @param peer Peer to add.
     */
    void addPeer(PeerId peer);
    /**
     * @brief Removes a peer from the group. Its events still queued are kept: they arrived while it was a member.
     * @param peer Peer to remove.
     */
    void removePeer(PeerId peer);

    /**
     * @brief Queues an event for the engine's next `poll()`; dropped if its peer is not in the group.
     *
     * The queue is only emptied by `poll()`. A peer with `maxQueuedMessages` `Data` events waiting has its next ones
     * dropped and counted in `droppedMessages()`, so a flood while the engine does not poll cannot exhaust memory.
     * `Connected` and `Disconnected` are never dropped.
     *
     * @param event Event taken from the transport.
     */
    void push(NetworkEvent event);

    /**
     * @brief Messages dropped so far because their peer was over the cap.
     * @return The count, since the group was created.
     */
    [[nodiscard]] std::uint64_t droppedMessages() const noexcept { return _dropped.load(std::memory_order_relaxed); }

    /**
     * @brief Takes the events pushed since the last call.
     * @return Events in arrival order.
     */
    [[nodiscard]] std::vector<NetworkEvent> poll() override;

    /**
     * @brief Sends a message to a peer of the group; dropped for any other peer.
     * @param peer Destination peer.
     * @param data Payload.
     * @param channel Delivery guarantee.
     */
    void send(PeerId peer, const Bytes& data, Channel channel) override;
    /**
     * @brief Sends a message to every peer of the group.
     * @param data Payload.
     * @param channel Delivery guarantee.
     */
    void broadcast(const Bytes& data, Channel channel) override;
    /**
     * @brief Closes the connection of a peer of the group; ignored for any other peer.
     * @param peer Peer to disconnect.
     */
    void kick(PeerId peer) override;

  private:
    /** @brief Shared transport. */
    INetworkServer* _server;
    /** @brief Cap on the `Data` events waiting per peer. */
    std::size_t _maxQueuedMessages;
    /** @brief Messages dropped so far. */
    std::atomic<std::uint64_t> _dropped{0};
    /** @brief Protects `_peers` and `_events`: `INetworkService` is thread-safe. */
    std::mutex _mutex;
    /** @brief Peers of the group. */
    std::unordered_set<PeerId> _peers;
    /** @brief Events waiting for `poll()`. */
    std::vector<NetworkEvent> _events;
    /** @brief `Data` events waiting, per peer. */
    std::unordered_map<PeerId, std::size_t> _queuedPerPeer;
};

}  // namespace sq::net
