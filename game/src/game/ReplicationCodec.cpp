#include "game/ReplicationCodec.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "log/Logger.hpp"
#include "net/INetworkManager.hpp"
#include "net/IReplicationCodec.hpp"
#include "net/NetworkConfig.hpp"
#include "net/SnapshotApplier.hpp"
#include "net/components/InputBuffer.hpp"
#include "net/components/Replicated.hpp"
#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"

namespace rtype::game {

namespace {

constexpr const char* LOG_TAG = "replication";

// The engine's transport refuses larger datagrams: every snapshot that passes the size check can be sent.
static_assert(protocol::MAX_UNRELIABLE_SIZE <= sq::net::MAX_UNRELIABLE_PAYLOAD);
static_assert(protocol::MAX_SNAPSHOT_PARTS <= sq::net::MAX_SNAPSHOT_PARTS);

}  // namespace

void ReplicationCodec::beginSnapshot(const std::uint32_t tick) {
    _outgoing.tick = tick;
    _outgoing.entities.clear();
}

void ReplicationCodec::writeEntity(const sq::ecs::Registry& registry, const sq::ecs::Entity entity,
                                   const std::uint32_t id, const sq::ecs::Replicated& replicated) {
    sq::ecs::Transform transform;
    if (registry.hasComponent<sq::ecs::Transform>(entity)) {
        transform = registry.getComponents<sq::ecs::Transform>().get(entity);
    }
    if (!std::isfinite(transform.x) || !std::isfinite(transform.y)) {
        sq::engine::log::error(LOG_TAG, "entity ", id, " has a non-finite position: not replicated");
        return;
    }
    EntityNetState state;
    if (registry.hasComponent<EntityNetState>(entity)) {
        state = registry.getComponents<EntityNetState>().get(entity);
    }
    _outgoing.entities.push_back({
        .entityId = id,
        .kind = replicated.kind,
        .subtype = state.subtype,
        .x = transform.x,
        .y = transform.y,
        .health = state.health,
        .flags = state.flags,
    });
}

std::vector<sq::net::Bytes> ReplicationCodec::encodeSnapshot(const std::uint32_t lastProcessedInput) {
    const std::size_t entities = _outgoing.entities.size();
    // At least one part: an empty snapshot tells the client that its entities are gone.
    const std::size_t partCount =
        std::max<std::size_t>((entities + protocol::MAX_ENTITIES_PER_PART - 1) / protocol::MAX_ENTITIES_PER_PART, 1);
    if (partCount > protocol::MAX_SNAPSHOT_PARTS) {
        sq::engine::log::error(LOG_TAG, "snapshot of ", entities, " entities does not fit in ",
                               protocol::MAX_SNAPSHOT_PARTS, " parts: not sent");
        return {};
    }

    const auto at = [this, entities](const std::size_t part) {
        const std::size_t index = std::min(part * protocol::MAX_ENTITIES_PER_PART, entities);
        return _outgoing.entities.begin() + static_cast<std::ptrdiff_t>(index);
    };
    std::vector<sq::net::Bytes> parts;
    parts.reserve(partCount);
    protocol::WorldState part{.tick = _outgoing.tick,
                              .lastProcessedInput = lastProcessedInput,
                              .entities = {},
                              .partIndex = 0,
                              .partCount = static_cast<std::uint32_t>(partCount)};
    for (std::size_t index = 0; index < partCount; ++index) {
        part.entities.assign(at(index), at(index + 1));
        part.partIndex = static_cast<std::uint32_t>(index);
        parts.push_back(protocol::encode(part));
    }
    return parts;
}

std::optional<sq::ecs::NetInput> ReplicationCodec::decodeInput(const sq::net::Bytes& data) {
    const auto message = protocol::decode(data);
    const auto* input = message ? std::get_if<protocol::PlayerInput>(&*message) : nullptr;
    if (input == nullptr) {
        return std::nullopt;
    }
    sq::ecs::NetInput decoded{.sequence = input->sequence, .held = input->held, .axes = {}};
    decoded.axes.reserve(input->axes.size());
    for (const auto& [action, value] : input->axes) {
        decoded.axes.push_back({.action = action, .value = value});
    }
    return decoded;
}

sq::net::Bytes ReplicationCodec::encodeInput(const sq::ecs::NetInput& input) {
    protocol::PlayerInput encoded{.sequence = input.sequence, .held = input.held, .axes = {}};
    encoded.axes.reserve(input.axes.size());
    for (const auto& [action, value] : input.axes) {
        encoded.axes.push_back({.action = action, .value = value});
    }
    return protocol::encode(encoded);
}

std::optional<sq::net::SnapshotHeader> ReplicationCodec::decodeSnapshot(const sq::net::Bytes& data) {
    auto message = protocol::decode(data);
    auto* snapshot = message ? std::get_if<protocol::WorldState>(&*message) : nullptr;
    if (snapshot == nullptr) {
        return std::nullopt;
    }
    _incoming = std::move(*snapshot);
    return sq::net::SnapshotHeader{.tick = _incoming.tick,
                                   .lastProcessedInput = _incoming.lastProcessedInput,
                                   .partIndex = _incoming.partIndex,
                                   .partCount = _incoming.partCount};
}

void ReplicationCodec::applySnapshot(sq::net::SnapshotApplier& applier) {
    sq::ecs::Registry& registry = applier.registry();
    for (const protocol::EntityState& state : _incoming.entities) {
        const auto entity = applier.entity(state.entityId, state.kind);
        if (!entity) {
            continue;
        }
        sq::ecs::Transform transform;
        if (registry.hasComponent<sq::ecs::Transform>(*entity)) {
            transform = registry.getComponents<sq::ecs::Transform>().get(*entity);
        }
        transform.x = state.x;
        transform.y = state.y;
        applier.setTransform(*entity, transform);

        registry.addComponent(*entity,
                              EntityNetState{.subtype = state.subtype, .health = state.health, .flags = state.flags});
    }
}

}  // namespace rtype::game
