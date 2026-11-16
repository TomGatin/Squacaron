#include "systems/render/RenderSystem.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "render/api/components/CameraComponent.hpp"
#include "render/api/components/Hidden.hpp"
#include "render/api/components/Renderable.hpp"
#include "render/api/components/SpriteInstances.hpp"
#include "render/detail/RaylibStores.hpp"
#include "render/texture/TextureStore.hpp"
#include "systems/render/RenderSystemDefaults.hpp"
#include "systems/render/RenderSystemSort.hpp"

namespace sq::render {

RenderSystem::RenderSystem(const TextureStore& textures) : _textures(&textures), _camera() {
    _queue.reserve(detail::INITIAL_QUEUE_CAPACITY);
}

void RenderSystem::collect(sq::ecs::Registry& registry) {
    _queue.clear();
    const auto* hidden = static_cast<const sq::ecs::Registry&>(registry).findComponents<sq::ecs::Hidden>();
    registry.runSystem<sq::ecs::Transform, sq::ecs::Renderable>([this, hidden](sq::ecs::Entity entity,
                                                                               const sq::ecs::Transform& transform,
                                                                               const sq::ecs::Renderable& renderable) {
        if (hidden != nullptr && hidden->contains(entity)) {
            return;
        }
        _queue.push_back(DrawItem{
            .entity = entity,
            .layer = renderable.layer,
            .textureId = renderable.textureId,
            .sourceRect = renderable.sourceRect,
            .origin = renderable.origin,
            .transform = transform,
            .tint = renderable.tint,
        });
    });
    registry.runSystem<sq::ecs::SpriteInstances, sq::ecs::Renderable>(
        [this, hidden](sq::ecs::Entity entity, const sq::ecs::SpriteInstances& pool,
                       const sq::ecs::Renderable& renderable) {
            if (hidden != nullptr && hidden->contains(entity)) {
                return;
            }
            for (const auto& inst : pool.instances) {
                _queue.push_back(DrawItem{
                    .entity = entity,
                    .layer = renderable.layer,
                    .textureId = renderable.textureId,
                    .sourceRect = renderable.sourceRect,
                    .origin = renderable.origin,
                    .transform =
                        sq::ecs::Transform{
                            .x = inst.x,
                            .y = inst.y,
                            .z = pool.z,
                            .rotation = pool.rotation,
                            .scaleX = pool.scaleX,
                            .scaleY = pool.scaleY,
                        },
                    .tint = renderable.tint,
                });
            }
        });

    std::ranges::sort(_queue, detail::drawsBefore);

    _camera = CameraView{};
    bool cameraFound = false;
    registry.runSystem<sq::ecs::CameraComponent>(
        [this, &cameraFound](sq::ecs::Entity /*entity*/, const sq::ecs::CameraComponent& component) {
            if (!cameraFound && component.active) {
                _camera = CameraView{
                    .offset = {.x = component.offsetX, .y = component.offsetY},
                    .target = {.x = component.targetX, .y = component.targetY},
                    .rotation = component.rotation,
                    .zoom = component.zoom,
                };
                cameraFound = true;
            }
        });
}

void RenderSystem::draw() const {
    BeginMode2D(Camera2D{
        .offset = {.x = _camera.offset.x, .y = _camera.offset.y},
        .target = {.x = _camera.target.x, .y = _camera.target.y},
        .rotation = _camera.rotation,
        .zoom = _camera.zoom,
    });
    for (const DrawItem& item : _queue) {
        drawItem(item);
    }
    EndMode2D();
}

void RenderSystem::update(sq::ecs::Registry& registry) {
    collect(registry);
    draw();
}

void RenderSystem::drawItem(const DrawItem& item) const {
    const sq::ecs::Transform& transform = item.transform;
    const Color tint{.r = item.tint.r, .g = item.tint.g, .b = item.tint.b, .a = item.tint.a};
    const float degrees = transform.rotation * detail::RADIANS_TO_DEGREES;

    if (item.textureId == sq::ecs::NO_TEXTURE) {
        const float width = detail::PLACEHOLDER_SIZE * std::fabs(transform.scaleX);
        const float height = detail::PLACEHOLDER_SIZE * std::fabs(transform.scaleY);
        const Rectangle rect{.x = transform.x, .y = transform.y, .width = width, .height = height};
        DrawRectanglePro(rect, Vector2{.x = width * item.origin.x, .y = height * item.origin.y}, degrees, tint);
        return;
    }

    const Texture2D* texture = _textures->impl().get(item.textureId);
    if (texture == nullptr) {
        return;
    }

    const auto textureWidth = static_cast<float>(texture->width);
    const auto textureHeight = static_cast<float>(texture->height);
    const bool usesWholeTexture = item.sourceRect.width == 0 && item.sourceRect.height == 0;
    const float sourceX = usesWholeTexture ? 0.0F : static_cast<float>(item.sourceRect.x);
    const float sourceY = usesWholeTexture ? 0.0F : static_cast<float>(item.sourceRect.y);
    const float sourceWidth = usesWholeTexture ? textureWidth : static_cast<float>(item.sourceRect.width);
    const float sourceHeight = usesWholeTexture ? textureHeight : static_cast<float>(item.sourceRect.height);
    // A negative source size mirrors the texture, which is how negative scales flip a sprite.
    const Rectangle source{
        .x = sourceX,
        .y = sourceY,
        .width = transform.scaleX < 0.0F ? -sourceWidth : sourceWidth,
        .height = transform.scaleY < 0.0F ? -sourceHeight : sourceHeight,
    };
    const float destWidth = sourceWidth * std::fabs(transform.scaleX);
    const float destHeight = sourceHeight * std::fabs(transform.scaleY);
    const Rectangle dest{.x = transform.x, .y = transform.y, .width = destWidth, .height = destHeight};
    DrawTexturePro(*texture, source, dest, Vector2{.x = destWidth * item.origin.x, .y = destHeight * item.origin.y},
                   degrees, tint);
}

}  // namespace sq::render
