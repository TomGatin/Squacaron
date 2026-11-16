#include "net/InputQueue.hpp"

#include <algorithm>
#include <iterator>
#include <optional>
#include <utility>

#include "net/components/InputBuffer.hpp"

namespace sq::net {

bool pushInput(ecs::InputBuffer& buffer, ecs::NetInput input) {
    if (input.sequence <= buffer.lastProcessed || buffer.capacity == 0) {
        return false;
    }
    auto& queue = buffer.pending;
    if (queue.size() >= buffer.capacity && input.sequence < queue.front().sequence) {
        return false;
    }
    const auto pos = std::ranges::upper_bound(queue, input.sequence, {}, &ecs::NetInput::sequence);
    if (pos != queue.begin() && std::prev(pos)->sequence == input.sequence) {
        return false;
    }
    queue.insert(pos, std::move(input));
    while (queue.size() > buffer.capacity) {
        queue.pop_front();
    }
    return true;
}

std::optional<ecs::NetInput> popInput(ecs::InputBuffer& buffer) {
    if (buffer.pending.empty()) {
        return std::nullopt;
    }
    ecs::NetInput input = std::move(buffer.pending.front());
    buffer.pending.pop_front();
    buffer.lastProcessed = input.sequence;
    return input;
}

}  // namespace sq::net
