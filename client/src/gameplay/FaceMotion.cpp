#include "gameplay/FaceMotion.hpp"

#include <cmath>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"

namespace client::gameplay {

void faceMotion(sq::ecs::Registry& registry) {
    registry.runSystem<FaceMotion, sq::ecs::Transform>(
        [](sq::ecs::Entity /*entity*/, FaceMotion& facing, sq::ecs::Transform& transform) {
            const float dx = transform.x - facing.lastX;
            const float dy = transform.y - facing.lastY;
            if (facing.seen && std::hypot(dx, dy) >= MIN_FACING_MOVE) {
                // Rotating by (direction - forward) brings the side the sprite faces onto the direction.
                transform.rotation = std::atan2(dy, dx) - facing.forward;
            }
            facing.lastX = transform.x;
            facing.lastY = transform.y;
            facing.seen = true;
        });
}

}  // namespace client::gameplay
