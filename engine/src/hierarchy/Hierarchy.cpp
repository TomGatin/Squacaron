#include "hierarchy/Hierarchy.hpp"

#include <cstddef>
#include <optional>
#include <span>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "hierarchy/HierarchyComponents.hpp"
#include "hierarchy/TransformMath.hpp"
#include "hierarchy/detail/LinkEditScope.hpp"

namespace sq::hierarchy {

namespace {

/**
 * @brief Reads the world transform of an entity, identity if it has none.
 * @param registry Registry owning the entity.
 * @param e Entity to read.
 * @return Its `sq::ecs::Transform`, or a default one.
 */
sq::ecs::Transform worldOf(const sq::ecs::Registry& registry, sq::ecs::Entity e) {
    return registry.hasComponent<sq::ecs::Transform>(e) ? registry.getComponents<sq::ecs::Transform>().get(e)
                                                        : sq::ecs::Transform{};
}

/**
 * @brief Removes `child` from the `Children` list of `parent`, dropping the component once empty.
 * @param registry Registry owning the entities.
 * @param parent Parent whose list is edited.
 * @param child Child to remove from the list.
 */
void eraseFromChildren(sq::ecs::Registry& registry, sq::ecs::Entity parent, sq::ecs::Entity child) {
    if (!registry.hasComponent<Children>(parent)) {
        return;
    }
    auto& list = registry.getComponents<Children>().get(parent).entities;
    std::erase(list, child);
    if (list.empty()) {
        registry.removeComponent<Children>(parent);
    }
}

}  // namespace

bool setParent(sq::ecs::Registry& registry, sq::ecs::Entity child, sq::ecs::Entity parent) {
    if (!registry.isAlive(child) || !registry.isAlive(parent) || child == parent ||
        isAncestorOf(registry, child, parent)) {
        return false;
    }
    if (parentOf(registry, child) == parent) {
        return true;
    }
    removeParent(registry, child);

    const LocalTransform local = toLocal(worldOf(registry, parent), worldOf(registry, child));
    registry.addComponent<Parent>(child, Parent{.entity = parent});
    registry.addComponent<LocalTransform>(child, local);
    if (!registry.hasComponent<Children>(parent)) {
        registry.addComponent<Children>(parent, Children{});
    }
    registry.getComponents<Children>().get(parent).entities.push_back(child);
    return true;
}

void removeParent(sq::ecs::Registry& registry, sq::ecs::Entity child) {
    const std::optional<sq::ecs::Entity> parent = parentOf(registry, child);
    if (!parent.has_value()) {
        return;
    }
    // Both sides are repaired here, so the hooks' linear manual-removal scans are skipped.
    const detail::LinkEditScope scope;
    eraseFromChildren(registry, *parent, child);
    registry.removeComponent<Parent>(child);
    registry.removeComponent<LocalTransform>(child);
}

std::optional<sq::ecs::Entity> parentOf(const sq::ecs::Registry& registry, sq::ecs::Entity e) {
    if (!registry.hasComponent<Parent>(e)) {
        return std::nullopt;
    }
    return registry.getComponents<Parent>().get(e).entity;
}

std::span<const sq::ecs::Entity> childrenOf(const sq::ecs::Registry& registry, sq::ecs::Entity e) {
    if (!registry.hasComponent<Children>(e)) {
        return {};
    }
    return registry.getComponents<Children>().get(e).entities;
}

bool isAncestorOf(const sq::ecs::Registry& registry, sq::ecs::Entity ancestor, sq::ecs::Entity e) {
    // setParent rejects cycles, so the chain always ends; the bound only guards against hand-made links.
    std::optional<sq::ecs::Entity> current = parentOf(registry, e);
    for (std::size_t depth = 0; current.has_value() && depth <= registry.getComponents<Parent>().getDense().size();
         ++depth) {
        if (*current == ancestor) {
            return true;
        }
        current = parentOf(registry, *current);
    }
    return false;
}

sq::ecs::Entity rootOf(const sq::ecs::Registry& registry, sq::ecs::Entity e) {
    sq::ecs::Entity root = e;
    std::optional<sq::ecs::Entity> current = parentOf(registry, e);
    for (std::size_t depth = 0; current.has_value() && depth <= registry.getComponents<Parent>().getDense().size();
         ++depth) {
        root = *current;
        current = parentOf(registry, *current);
    }
    return root;
}

}  // namespace sq::hierarchy
