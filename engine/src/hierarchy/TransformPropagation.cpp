#include "hierarchy/TransformPropagation.hpp"

#include <vector>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "hierarchy/HierarchyComponents.hpp"
#include "hierarchy/TransformMath.hpp"

namespace sq::hierarchy {

namespace {

/**
 * @brief Checks whether an entity is the root of a tree that needs propagating.
 * @param registry Registry holding the hierarchy.
 * @param e Entity owning a `Children` component.
 * @return True if `e` has no parent, or a parent that is not alive.
 */
bool isRoot(const sq::ecs::Registry& registry, sq::ecs::Entity e) {
    if (!registry.hasComponent<Parent>(e)) {
        return true;
    }
    return !registry.isAlive(registry.getComponents<Parent>().get(e).entity);
}

}  // namespace

void propagateTransforms(sq::ecs::Registry& registry) {
    auto& childrenArray = registry.getComponents<Children>();
    if (childrenArray.getDense().empty()) {
        return;
    }
    auto& transforms = registry.getComponents<sq::ecs::Transform>();
    auto& locals = registry.getComponents<LocalTransform>();

    // Parents are pushed only after their own world transform was written, so children always read an up-to-date one.
    std::vector<sq::ecs::Entity> stack;
    for (const sq::ecs::Entity e : childrenArray.getDenseEntities()) {
        if (isRoot(registry, e)) {
            stack.push_back(e);
        }
    }

    while (!stack.empty()) {
        const sq::ecs::Entity parent = stack.back();
        stack.pop_back();
        const sq::ecs::Transform parentWorld =
            transforms.contains(parent) ? transforms.get(parent) : sq::ecs::Transform{};

        if (!childrenArray.contains(parent)) {
            continue;
        }
        for (const sq::ecs::Entity child : childrenArray.get(parent).entities) {
            if (locals.contains(child)) {
                const sq::ecs::Transform world = compose(parentWorld, locals.get(child));
                if (transforms.contains(child)) {
                    transforms.get(child) = world;
                } else {
                    registry.addComponent<sq::ecs::Transform>(child, world);
                }
            }
            if (childrenArray.contains(child)) {
                stack.push_back(child);
            }
        }
    }
}

}  // namespace sq::hierarchy
