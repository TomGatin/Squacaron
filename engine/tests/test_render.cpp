#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "render/api/components/AnimationComponent.hpp"
#include "render/api/components/CameraComponent.hpp"
#include "render/api/components/Hidden.hpp"
#include "render/api/components/Renderable.hpp"
#include "render/texture/TextureStore.hpp"
#include "render/window/Window.hpp"
#include "systems/render/AnimationSystem.hpp"
#include "systems/render/RenderSystem.hpp"

TEST(CameraComponent, DefaultValues) {
    const sq::ecs::CameraComponent component;

    EXPECT_TRUE(component.active);
    EXPECT_EQ(component.zoom, 1.0F);
    EXPECT_EQ(component.rotation, 0.0F);
    EXPECT_EQ(component.targetX, 0.0F);
    EXPECT_EQ(component.targetY, 0.0F);
    EXPECT_EQ(component.offsetX, 0.0F);
    EXPECT_EQ(component.offsetY, 0.0F);
}

TEST(CameraComponent, WorksAsEcsComponent) {
    sq::ecs::Registry registry;
    const sq::ecs::Entity entity = registry.spawnEntity();

    sq::ecs::CameraComponent component{.targetX = 400.0F, .targetY = 300.0F, .zoom = 2.0F};
    registry.addComponent<sq::ecs::CameraComponent>(entity, component);

    ASSERT_TRUE(registry.hasComponent<sq::ecs::CameraComponent>(entity));
    const auto& stored = registry.getComponents<sq::ecs::CameraComponent>().get(entity);
    EXPECT_EQ(stored.targetX, 400.0F);
    EXPECT_EQ(stored.zoom, 2.0F);
}

namespace {

sq::ecs::Entity spawnDrawable(sq::ecs::Registry& registry, int layer, float z,
                              sq::ecs::TextureId textureId = sq::ecs::NO_TEXTURE) {
    const sq::ecs::Entity entity = registry.spawnEntity();
    registry.addComponent<sq::ecs::Transform>(entity, sq::ecs::Transform{.z = z});
    registry.addComponent<sq::ecs::Renderable>(entity, sq::ecs::Renderable{.textureId = textureId, .layer = layer});
    return entity;
}

std::vector<sq::ecs::Entity> drawOrder(const sq::render::RenderSystem& system) {
    std::vector<sq::ecs::Entity> order;
    for (const sq::render::DrawItem& item : system.drawQueue()) {
        order.push_back(item.entity);
    }
    return order;
}

}  // namespace

TEST(RenderSystem, SortsByLayerThenDepthThenTextureThenEntity) {
    sq::ecs::Registry registry;
    const sq::render::TextureStore textures;
    sq::render::RenderSystem system(textures);

    const sq::ecs::Entity front = spawnDrawable(registry, 2, 0.0F);
    const sq::ecs::Entity middleNear = spawnDrawable(registry, 1, 5.0F);
    const sq::ecs::Entity middleFar = spawnDrawable(registry, 1, -5.0F);
    const sq::ecs::Entity back = spawnDrawable(registry, 0, 9.0F);
    const sq::ecs::Entity tieSecond = spawnDrawable(registry, 3, 1.0F);
    const sq::ecs::Entity tieFirst = registry.spawnEntity();
    registry.addComponent<sq::ecs::Transform>(tieFirst, sq::ecs::Transform{.z = 1.0F});
    registry.addComponent<sq::ecs::Renderable>(tieFirst, sq::ecs::Renderable{.layer = 3});

    system.collect(registry);

    const std::vector<sq::ecs::Entity> expected{back, middleFar, middleNear, front, tieSecond, tieFirst};
    EXPECT_EQ(drawOrder(system), expected);
}

TEST(RenderSystem, GroupsTexturesOnlyAtMatchingLayerAndDepth) {
    sq::ecs::Registry registry;
    const sq::render::TextureStore textures;
    sq::render::RenderSystem system(textures);

    const sq::ecs::Entity textureNine = spawnDrawable(registry, 1, 2.0F, 9);
    const sq::ecs::Entity textureThreeFirst = spawnDrawable(registry, 1, 2.0F, 3);
    const sq::ecs::Entity textureThreeSecond = spawnDrawable(registry, 1, 2.0F, 3);
    const sq::ecs::Entity otherDepth = spawnDrawable(registry, 1, 1.0F, 9);
    const sq::ecs::Entity otherLayer = spawnDrawable(registry, 0, 2.0F, 9);

    system.collect(registry);

    const std::vector<sq::ecs::Entity> expected{otherLayer, otherDepth, textureThreeFirst, textureThreeSecond,
                                                textureNine};
    EXPECT_EQ(drawOrder(system), expected);
}

TEST(RenderSystem, IgnoresEntitiesMissingAComponent) {
    sq::ecs::Registry registry;
    const sq::render::TextureStore textures;
    sq::render::RenderSystem system(textures);

    const sq::ecs::Entity drawable = spawnDrawable(registry, 0, 0.0F);
    const sq::ecs::Entity onlyTransform = registry.spawnEntity();
    registry.addComponent<sq::ecs::Transform>(onlyTransform, sq::ecs::Transform{});
    const sq::ecs::Entity onlyRenderable = registry.spawnEntity();
    registry.addComponent<sq::ecs::Renderable>(onlyRenderable, sq::ecs::Renderable{});

    system.collect(registry);

    EXPECT_EQ(drawOrder(system), std::vector<sq::ecs::Entity>{drawable});
}

TEST(RenderSystem, CopiesRenderableDataIntoTheQueue) {
    sq::ecs::Registry registry;
    const sq::render::TextureStore textures;
    sq::render::RenderSystem system(textures);

    const sq::ecs::Entity entity = registry.spawnEntity();
    registry.addComponent<sq::ecs::Transform>(entity, sq::ecs::Transform{.x = 100.0F, .y = 200.0F});
    registry.addComponent<sq::ecs::Renderable>(
        entity, sq::ecs::Renderable{.textureId = 4,
                                    .sourceRect = {.x = 32, .y = 16, .width = 24, .height = 24},
                                    .origin = {.x = 0.0F, .y = 1.0F},
                                    .tint = {.r = 10, .g = 20, .b = 30, .a = 40},
                                    .layer = 1});

    system.collect(registry);

    ASSERT_EQ(system.drawQueue().size(), 1U);
    const sq::render::DrawItem& item = system.drawQueue().front();
    EXPECT_EQ(item.transform.x, 100.0F);
    EXPECT_EQ(item.transform.y, 200.0F);
    EXPECT_EQ(item.textureId, 4U);
    EXPECT_EQ(item.sourceRect.x, 32);
    EXPECT_EQ(item.sourceRect.y, 16);
    EXPECT_EQ(item.sourceRect.width, 24);
    EXPECT_EQ(item.sourceRect.height, 24);
    EXPECT_EQ(item.origin.x, 0.0F);
    EXPECT_EQ(item.origin.y, 1.0F);
    EXPECT_EQ(item.tint.b, 30);
    EXPECT_EQ(item.layer, 1);
}

TEST(RenderSystem, QueueIsRebuiltEachFrameWithoutReallocating) {
    sq::ecs::Registry registry;
    const sq::render::TextureStore textures;
    sq::render::RenderSystem system(textures);
    for (int i = 0; i < 200; ++i) {
        spawnDrawable(registry, i % 3, 0.0F);
    }

    system.collect(registry);
    const sq::render::DrawItem* firstFrameStorage = system.drawQueue().data();
    system.collect(registry);

    EXPECT_EQ(system.drawQueue().size(), 200U);
    EXPECT_EQ(system.drawQueue().data(), firstFrameStorage);
}

TEST(RenderSystem, UsesTheFirstActiveCamera) {
    sq::ecs::Registry registry;
    const sq::render::TextureStore textures;
    sq::render::RenderSystem system(textures);

    const sq::ecs::Entity inactive = registry.spawnEntity();
    sq::ecs::CameraComponent inactiveCamera;
    inactiveCamera.active = false;
    inactiveCamera.zoom = 9.0F;
    registry.addComponent<sq::ecs::CameraComponent>(inactive, inactiveCamera);

    const sq::ecs::Entity active = registry.spawnEntity();
    sq::ecs::CameraComponent activeCamera;
    activeCamera.zoom = 2.0F;
    registry.addComponent<sq::ecs::CameraComponent>(active, activeCamera);

    system.collect(registry);

    EXPECT_EQ(system.camera().zoom, 2.0F);
}

TEST(RenderSystem, FallsBackToANeutralCameraWhenNoneIsActive) {
    sq::ecs::Registry registry;
    const sq::render::TextureStore textures;
    sq::render::RenderSystem system(textures);

    system.collect(registry);

    EXPECT_EQ(system.camera().zoom, 1.0F);
    EXPECT_EQ(system.camera().target.x, 0.0F);
}

TEST(TextureStore, UnknownIdentifiersResolveToNothing) {
    const sq::render::TextureStore textures;

    EXPECT_FALSE(textures.contains(sq::ecs::NO_TEXTURE));
    EXPECT_FALSE(textures.contains(1));
}

TEST(TextureStore, UnloadIgnoresUnknownIdentifiersAndThePlaceholderNeedsAWindow) {
    sq::render::TextureStore textures;

    textures.unload(sq::ecs::NO_TEXTURE);
    textures.unload(42);
    EXPECT_EQ(textures.placeholder(), sq::ecs::NO_TEXTURE);
    EXPECT_EQ(textures.loadedCount(), 0U);
}

TEST(Window, RejectsNonPositiveDimensions) {
    EXPECT_THROW(sq::render::Window(sq::render::WindowConfig{.width = 0, .height = 720}), std::invalid_argument);
    EXPECT_THROW(sq::render::Window(sq::render::WindowConfig{.width = 1280, .height = -1}), std::invalid_argument);
}

TEST(Window, RejectsNonPositiveTargetFps) {
    EXPECT_THROW(sq::render::Window(sq::render::WindowConfig{.targetFps = 0}), std::invalid_argument);
    EXPECT_THROW(sq::render::Window(sq::render::WindowConfig{.targetFps = -1}), std::invalid_argument);
}

TEST(AnimationSystem, FramesThatTouchFollowEachOther) {
    sq::ecs::Registry registry;
    const sq::ecs::Entity entity = registry.spawnEntity();
    registry.addComponent(entity, sq::ecs::Renderable{});
    registry.addComponent(entity,
                          sq::ecs::AnimationComponent{
                              .frameWidth = 10, .frameHeight = 8, .frameCount = 3, .startX = 4, .frameDuration = 1.0F});
    sq::systems::AnimationSystem::update(registry, 1.0F);
    EXPECT_EQ(registry.getComponents<sq::ecs::Renderable>().get(entity).sourceRect.x, 14);
}

TEST(AnimationSystem, FrameStrideSkipsTheSpaceBetweenFrames) {
    sq::ecs::Registry registry;
    const sq::ecs::Entity entity = registry.spawnEntity();
    registry.addComponent(entity, sq::ecs::Renderable{});
    registry.addComponent(entity, sq::ecs::AnimationComponent{.frameWidth = 48,
                                                              .frameHeight = 48,
                                                              .frameCount = 5,
                                                              .startX = 0,
                                                              .frameStride = 65,
                                                              .frameDuration = 1.0F});
    sq::systems::AnimationSystem::update(registry, 1.0F);
    sq::systems::AnimationSystem::update(registry, 1.0F);
    const auto& source = registry.getComponents<sq::ecs::Renderable>().get(entity).sourceRect;
    EXPECT_EQ(source.x, 130);
    EXPECT_EQ(source.width, 48);
}

TEST(AnimationSystem, FramesOfDifferentWidthsPlayOnceThenHold) {
    sq::ecs::Registry registry;
    const sq::ecs::Entity entity = registry.spawnEntity();
    registry.addComponent(entity, sq::ecs::Renderable{});
    registry.addComponent(
        entity,
        sq::ecs::AnimationComponent{
            .frameHeight = 16, .startX = 151, .frameWidths = {63, 66, 16}, .frameDuration = 1.0F, .loop = false});
    const auto& source = registry.getComponents<sq::ecs::Renderable>().get(entity).sourceRect;
    sq::systems::AnimationSystem::update(registry, 0.5F);
    EXPECT_EQ(source.x, 151);
    EXPECT_EQ(source.width, 63);
    sq::systems::AnimationSystem::update(registry, 1.0F);
    EXPECT_EQ(source.x, 214);
    EXPECT_EQ(source.width, 66);
    sq::systems::AnimationSystem::update(registry, 1.0F);
    sq::systems::AnimationSystem::update(registry, 1.0F);
    EXPECT_EQ(source.x, 280) << "holds the last frame";
    EXPECT_EQ(source.width, 16);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(RenderSystem, HiddenEntitiesAreNotCollected) {
    sq::ecs::Registry registry;
    sq::render::TextureStore textures;
    sq::render::RenderSystem system(textures);

    const sq::ecs::Entity shown = registry.spawnEntity();
    const sq::ecs::Entity hidden = registry.spawnEntity();
    for (const sq::ecs::Entity entity : {shown, hidden}) {
        registry.addComponent<sq::ecs::Transform>(entity, sq::ecs::Transform{});
        registry.addComponent<sq::ecs::Renderable>(entity, sq::ecs::Renderable{.textureId = 1});
    }
    sq::ecs::hide(registry, hidden, sq::ecs::HiddenReason::Scene);

    system.collect(registry);
    ASSERT_EQ(system.drawQueue().size(), 1U);
    EXPECT_EQ(system.drawQueue().front().entity, shown);

    sq::ecs::show(registry, hidden, sq::ecs::HiddenReason::Scene);
    system.collect(registry);
    EXPECT_EQ(system.drawQueue().size(), 2U);
}
