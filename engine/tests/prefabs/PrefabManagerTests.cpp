#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/Mediator.hpp"
#include "core/MediatorError.hpp"
#include "ecs/CommandBuffer.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabManager.hpp"
#include "prefabs/PrefabParams.hpp"
#include "prefabs/PrefabPlugin.hpp"

namespace {

constexpr char const* SHOT_PREFAB = "shot";

/** A small prefab built from the transform keys and a `speed` parameter, as a game prefab would be. */
void buildShot(ecs::Entity e, ecs::Registry& registry, prefabs::PrefabParams const& params) {
    registry.addComponent<ecs::Transform>(e, prefabs::makeTransform(params));
    registry.addComponent<ecs::RigidBody2D>(e, ecs::RigidBody2D{.velocityX = params.get("speed", -400.0F)});
}

TEST(PrefabManager, UnknownPrefabQueuesNothing) {
    prefabs::PrefabManager manager;
    ecs::CommandBuffer buffer;

    EXPECT_FALSE(manager.has("does_not_exist"));
    EXPECT_FALSE(manager.spawn("does_not_exist", buffer));
    EXPECT_TRUE(buffer.empty());
}

TEST(PrefabManager, SpawnIsDeferredUntilFlush) {
    prefabs::PrefabManager manager;
    manager.registerPrefab(SHOT_PREFAB, buildShot);
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    ASSERT_TRUE(manager.spawn(SHOT_PREFAB, buffer));
    EXPECT_FALSE(buffer.empty());
    EXPECT_FALSE(registry.isAlive(0));

    buffer.flush(registry);

    EXPECT_TRUE(buffer.empty());
    EXPECT_TRUE(registry.isAlive(0));
}

TEST(PrefabParams, NumbersConvertOtherTypesFallBack) {
    prefabs::PrefabParams params{{"speed", 10}, {"scale", 2.5F}, {"tag", "boss"}, {"visible", true}};

    EXPECT_FLOAT_EQ(params.get("speed", 0.0F), 10.0F);
    EXPECT_EQ(params.get("scale", 0), 2);
    EXPECT_EQ(params.get<std::string>("tag", ""), "boss");
    EXPECT_TRUE(params.get("visible", false));
    EXPECT_FLOAT_EQ(params.get("tag", 1.0F), 1.0F);
    EXPECT_FLOAT_EQ(params.get("missing", 3.0F), 3.0F);
    EXPECT_FALSE(params.find<bool>("speed").has_value());
}

TEST(PrefabParams, MergeOverridesExistingKeys) {
    prefabs::PrefabParams params{{"x", 1.0F}, {"y", 2.0F}};
    params.merge(prefabs::PrefabParams{}.set("y", 5.0F).set("rotation", 0.5F));

    EXPECT_FLOAT_EQ(params.get("x", 0.0F), 1.0F);
    EXPECT_FLOAT_EQ(params.get("y", 0.0F), 5.0F);
    EXPECT_FLOAT_EQ(params.get("rotation", 0.0F), 0.5F);
}

TEST(PrefabManager, SpawnParamsOverrideRegistrationDefaults) {
    prefabs::PrefabManager manager;
    manager.registerPrefab("fast_shot", buildShot, {{"speed", -1200.0F}, {"y", 50.0F}});
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    manager.spawn("fast_shot", buffer, {{"x", 10.0F}, {"rotation", 1.5F}, {"scaleX", -1.0F}, {"speed", -300.0F}});
    buffer.flush(registry);

    auto const& transform = registry.getComponents<ecs::Transform>().get(0);
    EXPECT_FLOAT_EQ(transform.x, 10.0F);
    EXPECT_FLOAT_EQ(transform.y, 50.0F);
    EXPECT_FLOAT_EQ(transform.rotation, 1.5F);
    EXPECT_FLOAT_EQ(transform.scaleX, -1.0F);
    EXPECT_FLOAT_EQ(registry.getComponents<ecs::RigidBody2D>().get(0).velocityX, -300.0F);
}

TEST(PrefabManager, SpawningSurvivesManagerDestructionBeforeFlush) {
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    {
        prefabs::PrefabManager localManager;
        localManager.registerPrefab(SHOT_PREFAB, buildShot);
        ASSERT_TRUE(localManager.spawn(SHOT_PREFAB, buffer, {{"x", 123.0F}}));
    }

    buffer.flush(registry);

    ASSERT_TRUE(registry.hasComponent<ecs::Transform>(0));
    EXPECT_FLOAT_EQ(registry.getComponents<ecs::Transform>().get(0).x, 123.0F);
}

}  // namespace

TEST(PrefabPlugin, InitFailsAndKeepsExistingManagerOnDuplicate) {
    engine::Engine engine;
    auto const existing = std::make_shared<prefabs::PrefabManager>();
    ASSERT_TRUE(engine.mediator().registerService<prefabs::PrefabManager>(existing).has_value());

    engine.addPlugin<prefabs::PrefabPlugin>();

    EXPECT_THROW(engine.init(), engine::MediatorError);
    EXPECT_EQ(&engine.mediator().service<prefabs::PrefabManager>(), existing.get());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
