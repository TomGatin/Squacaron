#include "net/SnapshotApplier.hpp"

#include <cstdint>
#include <optional>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "net/NetworkEntityMap.hpp"
#include "net/components/Interpolated.hpp"
#include "net/components/NetworkId.hpp"
#include "net/components/Predicted.hpp"
#include "net/components/Replicated.hpp"

namespace sq::net {

SnapshotApplier::SnapshotApplier(sq::ecs::Registry& registry, NetworkEntityMap& map, const float interval)
    : _registry(&registry), _map(&map), _interval(interval) {}

std::optional<sq::ecs::Entity> SnapshotApplier::entity(const std::uint32_t id, const std::uint32_t kind) {
    if (id == 0) {
        return std::nullopt;
    }
    if (const auto existing = _map->entityOf(id); existing && _registry->isAlive(*existing)) {
        _seen.insert(*existing);
        return existing;
    }
    const sq::ecs::Entity created = _registry->spawnEntity();
    _map->bind(id, created);
    _registry->emplaceComponent<sq::ecs::NetworkId>(created, id);
    _registry->emplaceComponent<sq::ecs::Replicated>(created, kind);
    _seen.insert(created);
    _spawned.push_back({.entity = created, .id = id, .kind = kind});
    return created;
}

void SnapshotApplier::setTransform(const sq::ecs::Entity entity, const sq::ecs::Transform& transform) {
    if (!_registry->isAlive(entity)) {
        return;
    }
    // A predicted entity is reset to the server state at once, then reconciled: interpolating it would lag behind.
    if (_registry->hasComponent<sq::ecs::Interpolated>(entity) &&
        !_registry->hasComponent<sq::ecs::Predicted>(entity)) {
        auto& interp = _registry->getComponents<sq::ecs::Interpolated>().get(entity);
        const sq::ecs::Transform current = _registry->hasComponent<sq::ecs::Transform>(entity)
                                               ? _registry->getComponents<sq::ecs::Transform>().get(entity)
                                               : transform;
        interp.from = interp.initialized ? current : transform;
        interp.to = transform;
        interp.elapsed = 0.0F;
        interp.duration = interp.initialized ? _interval : 0.0F;
        interp.initialized = true;
        if (!_registry->hasComponent<sq::ecs::Transform>(entity)) {
            _registry->addComponent(entity, transform);
        }
        return;
    }
    if (_registry->hasComponent<sq::ecs::Transform>(entity)) {
        _registry->getComponents<sq::ecs::Transform>().get(entity) = transform;
    } else {
        _registry->addComponent(entity, transform);
    }
}

}  // namespace sq::net
