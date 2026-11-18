#include "net/asio/EventQueue.hpp"

#include <cstddef>
#include <mutex>
#include <utility>
#include <vector>

#include "log/Logger.hpp"
#include "net/INetworkManager.hpp"
#include "net/asio/LogTag.hpp"

namespace sq::net::detail {

bool EventQueue::push(NetworkEvent event) {
    const std::scoped_lock lock(_mutex);
    if (event.type == EventType::Data) {
        std::size_t& queued = _dataPerPeer[event.peer];
        if (queued >= _maxDataPerPeer) {
            // Logged once per full queue: the next drops of this peer, until the game polls, are only counted.
            if (queued == _maxDataPerPeer) {
                sq::engine::log::warning(LOG_TAG, "peer ", event.peer, " has ", _maxDataPerPeer,
                                         " messages waiting for the game: dropping its next ones");
                ++queued;
            }
            _counters->onEventDropped();
            return false;
        }
        ++queued;
    }
    _events.push_back(std::move(event));
    return true;
}

std::vector<NetworkEvent> EventQueue::popAll() {
    std::vector<NetworkEvent> events;
    {
        const std::scoped_lock lock(_mutex);
        events.swap(_events);
        _dataPerPeer.clear();
    }
    return events;
}

std::size_t EventQueue::size() {
    const std::scoped_lock lock(_mutex);
    return _events.size();
}

}  // namespace sq::net::detail
