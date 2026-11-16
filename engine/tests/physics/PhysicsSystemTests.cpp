#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <vector>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "events/EventBus.hpp"
#include "systems/physics/PhysicsEvents.hpp"
#include "systems/physics/PhysicsSystem.hpp"
#include "systems/physics/components/Collider2D.hpp"
#include "systems/physics/components/RigidBody2D.hpp"

namespace {

/** @brief Heap allocations made by this test binary since start (see the replaced operator new below). */
std::atomic<std::size_t> g_allocations{0};

}  // namespace

void* operator new(std::size_t size) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* ptr = std::malloc(size == 0 ? 1 : size)) {  // NOLINT(cppcoreguidelines-no-malloc)
        return ptr;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) { return ::operator new(size); }

void operator delete(void* ptr) noexcept { std::free(ptr); }  // NOLINT(cppcoreguidelines-no-malloc)

void operator delete[](void* ptr) noexcept { std::free(ptr); }  // NOLINT(cppcoreguidelines-no-malloc)

void operator delete(void* ptr, std::size_t /*size*/) noexcept { std::free(ptr); }  // NOLINT

void operator delete[](void* ptr, std::size_t /*size*/) noexcept { std::free(ptr); }  // NOLINT

namespace {

constexpr float FIXED_DT = 1.0F / 60.0F;

TEST(PhysicsSystem, DynamicBodyAdvancesTransform) {
    sq::ecs::Registry registry;
    sq::events::EventBus bus;
    sq::systems::PhysicsSystem physicsSystem(registry, bus);
    const sq::ecs::Entity entity = registry.spawnEntity();
    registry.addComponent<sq::ecs::Transform>(entity, sq::ecs::Transform{});
    registry.addComponent<sq::ecs::RigidBody2D>(
        entity, sq::ecs::RigidBody2D{.bodyType = sq::ecs::BodyType::Dynamic, .velocityX = 300.0F});

    physicsSystem.update(FIXED_DT);

    EXPECT_NEAR(registry.getComponents<sq::ecs::Transform>().get(entity).x, 5.0F, 0.001F);
    EXPECT_EQ(physicsSystem.bodyCount(), 1U);
}

TEST(PhysicsSystem, DynamicBodyStopsAtStaticWall) {
    sq::ecs::Registry registry;
    sq::events::EventBus bus;
    sq::systems::PhysicsSystem physicsSystem(registry, bus);

    const sq::ecs::Entity wall = registry.spawnEntity();
    registry.addComponent<sq::ecs::Transform>(wall, sq::ecs::Transform{.x = 100.0F});
    registry.addComponent<sq::ecs::RigidBody2D>(wall, sq::ecs::RigidBody2D{});
    registry.addComponent<sq::ecs::Collider2D>(wall, sq::ecs::Collider2D{.halfWidth = 5.0F, .halfHeight = 100.0F});

    const sq::ecs::Entity movingBody = registry.spawnEntity();
    registry.addComponent<sq::ecs::Transform>(movingBody, sq::ecs::Transform{});
    registry.addComponent<sq::ecs::RigidBody2D>(
        movingBody,
        sq::ecs::RigidBody2D{.bodyType = sq::ecs::BodyType::Dynamic, .velocityX = 600.0F, .maxLinearSpeed = 1000.0F});
    registry.addComponent<sq::ecs::Collider2D>(
        movingBody, sq::ecs::Collider2D{.shape = sq::ecs::ColliderShape::Circle, .radius = 8.0F});

    for (int step = 0; step < 60; ++step) {
        physicsSystem.update(FIXED_DT);
    }

    const sq::ecs::Transform& transform = registry.getComponents<sq::ecs::Transform>().get(movingBody);
    const sq::ecs::RigidBody2D& rigidBody = registry.getComponents<sq::ecs::RigidBody2D>().get(movingBody);
    EXPECT_LE(transform.x, 87.1F);
    EXPECT_NEAR(rigidBody.velocityX, 0.0F, 0.1F);
}

TEST(PhysicsSystem, ColliderScaleChangesCollisionBounds) {
    sq::ecs::Registry registry;
    sq::events::EventBus bus;
    sq::systems::PhysicsSystem physicsSystem(registry, bus);

    const sq::ecs::Entity wall = registry.spawnEntity();
    registry.addComponent<sq::ecs::Transform>(wall, sq::ecs::Transform{.x = 100.0F});
    registry.addComponent<sq::ecs::RigidBody2D>(wall, sq::ecs::RigidBody2D{});
    registry.addComponent<sq::ecs::Collider2D>(wall, sq::ecs::Collider2D{.halfWidth = 5.0F, .halfHeight = 100.0F});

    const sq::ecs::Entity movingBody = registry.spawnEntity();
    registry.addComponent<sq::ecs::Transform>(movingBody, sq::ecs::Transform{});
    registry.addComponent<sq::ecs::RigidBody2D>(
        movingBody,
        sq::ecs::RigidBody2D{.bodyType = sq::ecs::BodyType::Dynamic, .velocityX = 600.0F, .maxLinearSpeed = 1000.0F});
    registry.addComponent<sq::ecs::Collider2D>(
        movingBody, sq::ecs::Collider2D{.shape = sq::ecs::ColliderShape::Circle, .radius = 8.0F});

    physicsSystem.update(FIXED_DT);
    registry.getComponents<sq::ecs::Transform>().get(movingBody).scaleX = 2.0F;

    for (int step = 0; step < 60; ++step) {
        physicsSystem.update(FIXED_DT);
    }

    EXPECT_LE(registry.getComponents<sq::ecs::Transform>().get(movingBody).x, 79.1F);
}

TEST(PhysicsSystem, FastBulletDoesNotPassThroughThinWall) {
    sq::ecs::Registry registry;
    sq::events::EventBus bus;
    sq::systems::PhysicsSystem physicsSystem(registry, bus);

    const sq::ecs::Entity wall = registry.spawnEntity();
    registry.addComponent<sq::ecs::Transform>(wall, sq::ecs::Transform{.x = 100.0F});
    registry.addComponent<sq::ecs::RigidBody2D>(wall, sq::ecs::RigidBody2D{});
    registry.addComponent<sq::ecs::Collider2D>(wall, sq::ecs::Collider2D{.halfWidth = 1.0F, .halfHeight = 50.0F});

    const sq::ecs::Entity projectile = registry.spawnEntity();
    registry.addComponent<sq::ecs::Transform>(projectile, sq::ecs::Transform{});
    registry.addComponent<sq::ecs::RigidBody2D>(
        projectile,
        sq::ecs::RigidBody2D{
            .bodyType = sq::ecs::BodyType::Dynamic, .velocityX = 6000.0F, .maxLinearSpeed = 7000.0F, .bullet = true});
    registry.addComponent<sq::ecs::Collider2D>(
        projectile, sq::ecs::Collider2D{.shape = sq::ecs::ColliderShape::Circle, .radius = 2.0F});

    for (int step = 0; step < 4; ++step) {
        physicsSystem.update(FIXED_DT);
    }

    EXPECT_LE(registry.getComponents<sq::ecs::Transform>().get(projectile).x, 97.1F);
}

TEST(PhysicsSystem, EntityDestructionDestroysBody) {
    sq::ecs::Registry registry;
    sq::events::EventBus bus;
    sq::systems::PhysicsSystem physicsSystem(registry, bus);
    const sq::ecs::Entity entity = registry.spawnEntity();
    registry.addComponent<sq::ecs::Transform>(entity, sq::ecs::Transform{});
    registry.addComponent<sq::ecs::RigidBody2D>(entity, sq::ecs::RigidBody2D{});

    physicsSystem.update(FIXED_DT);
    ASSERT_EQ(physicsSystem.bodyCount(), 1U);

    registry.killEntity(entity);

    EXPECT_EQ(physicsSystem.bodyCount(), 0U);
}

TEST(PhysicsSystem, RemovingRigidBodyDestroysBody) {
    sq::ecs::Registry registry;
    sq::events::EventBus bus;
    sq::systems::PhysicsSystem physicsSystem(registry, bus);
    const sq::ecs::Entity entity = registry.spawnEntity();
    registry.addComponent<sq::ecs::Transform>(entity, sq::ecs::Transform{});
    registry.addComponent<sq::ecs::RigidBody2D>(entity, sq::ecs::RigidBody2D{});

    physicsSystem.update(FIXED_DT);
    ASSERT_EQ(physicsSystem.bodyCount(), 1U);

    registry.removeComponent<sq::ecs::RigidBody2D>(entity);

    EXPECT_EQ(physicsSystem.bodyCount(), 0U);
}

TEST(PhysicsSystem, RemovingTransformDestroysBody) {
    sq::ecs::Registry registry;
    sq::events::EventBus bus;
    sq::systems::PhysicsSystem physicsSystem(registry, bus);
    const sq::ecs::Entity entity = registry.spawnEntity();
    registry.addComponent<sq::ecs::Transform>(entity, sq::ecs::Transform{});
    registry.addComponent<sq::ecs::RigidBody2D>(entity, sq::ecs::RigidBody2D{});

    physicsSystem.update(FIXED_DT);
    ASSERT_EQ(physicsSystem.bodyCount(), 1U);

    // Without its Transform the body is no longer synchronized: it must not stay in the world and keep colliding.
    registry.removeComponent<sq::ecs::Transform>(entity);

    EXPECT_EQ(physicsSystem.bodyCount(), 0U);
}

constexpr std::uint16_t SHOT = 0x0001;
constexpr std::uint16_t TARGET = 0x0002;
constexpr std::uint16_t OTHER = 0x0004;

/** @brief Records every contact event published on a bus. */
struct ContactLog {
    std::vector<sq::events::CollisionBeginEvent> begins;
    std::vector<sq::events::CollisionEndEvent> ends;

    explicit ContactLog(sq::events::EventBus& bus) {
        bus.subscribe<sq::events::CollisionBeginEvent>(
            [this](const sq::events::CollisionBeginEvent& event) { begins.push_back(event); });
        bus.subscribe<sq::events::CollisionEndEvent>(
            [this](const sq::events::CollisionEndEvent& event) { ends.push_back(event); });
    }
};

sq::ecs::Entity spawnBody(sq::ecs::Registry& registry, float x, sq::ecs::RigidBody2D body,
                          sq::ecs::Collider2D collider) {
    const sq::ecs::Entity entity = registry.spawnEntity();
    registry.addComponent<sq::ecs::Transform>(entity, sq::ecs::Transform{.x = x});
    registry.addComponent<sq::ecs::RigidBody2D>(entity, body);
    registry.addComponent<sq::ecs::Collider2D>(entity, collider);
    return entity;
}

sq::ecs::Entity spawnTarget(sq::ecs::Registry& registry, float x, sq::ecs::BodyType type = sq::ecs::BodyType::Kinematic,
                            std::uint16_t mask = SHOT) {
    return spawnBody(
        registry, x, sq::ecs::RigidBody2D{.bodyType = type},
        sq::ecs::Collider2D{
            .halfWidth = 10.0F, .halfHeight = 10.0F, .categoryBits = TARGET, .maskBits = mask, .isSensor = true});
}

sq::ecs::Entity spawnShot(sq::ecs::Registry& registry, float x, float speed, std::uint16_t mask = TARGET) {
    return spawnBody(
        registry, x, sq::ecs::RigidBody2D{.bodyType = sq::ecs::BodyType::Dynamic, .velocityX = speed, .bullet = true},
        sq::ecs::Collider2D{
            .halfWidth = 2.0F, .halfHeight = 2.0F, .categoryBits = SHOT, .maskBits = mask, .isSensor = true});
}

void stepFor(sq::systems::PhysicsSystem& physicsSystem, int steps) {
    for (int step = 0; step < steps; ++step) {
        physicsSystem.update(FIXED_DT);
    }
}

TEST(PhysicsSystemContacts, SensorShotCrossingTargetPublishesBeginThenEnd) {
    sq::ecs::Registry registry;
    sq::events::EventBus bus;
    sq::systems::PhysicsSystem physicsSystem(registry, bus);
    ContactLog log(bus);

    const sq::ecs::Entity target = spawnTarget(registry, 100.0F);
    const sq::ecs::Entity shot = spawnShot(registry, 0.0F, 600.0F);

    stepFor(physicsSystem, 30);  // 300 px: enters at ~88 px, leaves at ~112 px

    ASSERT_EQ(log.begins.size(), 1U);
    ASSERT_EQ(log.ends.size(), 1U);
    const sq::events::CollisionBeginEvent& begin = log.begins.front();
    EXPECT_TRUE(begin.isSensor);
    const bool shotFirst = begin.entityA == shot;
    EXPECT_EQ(shotFirst ? begin.entityB : begin.entityA, target);
    EXPECT_EQ(shotFirst ? begin.categoryA : begin.categoryB, SHOT);
    EXPECT_EQ(shotFirst ? begin.categoryB : begin.categoryA, TARGET);
}

TEST(PhysicsSystemContacts, EventsCarryFullGenerationalHandles) {
    sq::ecs::Registry registry;
    sq::events::EventBus bus;
    sq::systems::PhysicsSystem physicsSystem(registry, bus);
    ContactLog log(bus);

    const sq::ecs::Entity firstDead = registry.spawnEntity();
    const sq::ecs::Entity secondDead = registry.spawnEntity();
    registry.killEntity(firstDead);
    registry.killEntity(secondDead);
    const sq::ecs::Entity target = spawnTarget(registry, 100.0F);
    const sq::ecs::Entity shot = spawnShot(registry, 0.0F, 600.0F);
    ASSERT_NE(sq::ecs::entityGeneration(target), 0U);
    ASSERT_NE(sq::ecs::entityGeneration(shot), 0U);

    stepFor(physicsSystem, 30);

    ASSERT_EQ(log.begins.size(), 1U);
    const sq::events::CollisionBeginEvent& begin = log.begins.front();
    const bool shotFirst = begin.entityA == shot;
    EXPECT_EQ(shotFirst ? begin.entityA : begin.entityB, shot);
    EXPECT_EQ(shotFirst ? begin.entityB : begin.entityA, target);
}

TEST(PhysicsSystemContacts, MaskFilteringSuppressesEvents) {
    sq::ecs::Registry registry;
    sq::events::EventBus bus;
    sq::systems::PhysicsSystem physicsSystem(registry, bus);
    ContactLog log(bus);

    spawnTarget(registry, 100.0F, sq::ecs::BodyType::Kinematic, OTHER);
    spawnShot(registry, 0.0F, 600.0F);

    stepFor(physicsSystem, 30);

    EXPECT_TRUE(log.begins.empty());
    EXPECT_TRUE(log.ends.empty());
}

TEST(PhysicsSystemContacts, TwoNonDynamicBodiesNeverCollide) {
    sq::ecs::Registry registry;
    sq::events::EventBus bus;
    sq::systems::PhysicsSystem physicsSystem(registry, bus);
    ContactLog log(bus);

    spawnTarget(registry, 0.0F, sq::ecs::BodyType::Kinematic, TARGET);
    spawnTarget(registry, 5.0F, sq::ecs::BodyType::Static, TARGET);

    stepFor(physicsSystem, 5);

    EXPECT_TRUE(log.begins.empty());
}

TEST(PhysicsSystemContacts, SolidContactIsNotASensor) {
    sq::ecs::Registry registry;
    sq::events::EventBus bus;
    sq::systems::PhysicsSystem physicsSystem(registry, bus);
    ContactLog log(bus);

    spawnBody(registry, 100.0F, sq::ecs::RigidBody2D{}, sq::ecs::Collider2D{.halfWidth = 5.0F, .halfHeight = 100.0F});
    spawnBody(registry, 0.0F, sq::ecs::RigidBody2D{.bodyType = sq::ecs::BodyType::Dynamic, .velocityX = 600.0F},
              sq::ecs::Collider2D{.shape = sq::ecs::ColliderShape::Circle, .radius = 8.0F});

    stepFor(physicsSystem, 60);

    ASSERT_FALSE(log.begins.empty());
    EXPECT_FALSE(log.begins.front().isSensor);
}

TEST(PhysicsSystemContacts, KillingAnEntityInAHandlerDropsItsOtherContacts) {
    sq::ecs::Registry registry;
    sq::events::EventBus bus;
    sq::systems::PhysicsSystem physicsSystem(registry, bus);

    // A wide target overlapped by two shots in the same step: the first hit kills the target.
    const sq::ecs::Entity target = spawnBody(
        registry, 100.0F, sq::ecs::RigidBody2D{.bodyType = sq::ecs::BodyType::Kinematic},
        sq::ecs::Collider2D{
            .halfWidth = 10.0F, .halfHeight = 50.0F, .categoryBits = TARGET, .maskBits = SHOT, .isSensor = true});
    const sq::ecs::Entity first = spawnShot(registry, 0.0F, 600.0F);
    const sq::ecs::Entity second = spawnShot(registry, 0.0F, 600.0F);
    registry.getComponents<sq::ecs::Transform>().get(second).y = 20.0F;

    std::vector<sq::events::CollisionBeginEvent> begins;
    bus.subscribe<sq::events::CollisionBeginEvent>([&](const sq::events::CollisionBeginEvent& event) {
        begins.push_back(event);
        if (registry.isAlive(target)) {
            registry.killEntity(target);
        }
    });

    EXPECT_NO_THROW(stepFor(physicsSystem, 30));

    ASSERT_EQ(begins.size(), 1U);
    const sq::ecs::Entity shot = begins.front().entityA == target ? begins.front().entityB : begins.front().entityA;
    EXPECT_TRUE(shot == first || shot == second);
    EXPECT_EQ(physicsSystem.bodyCount(), 2U);
}

TEST(PhysicsSystemContacts, DestroyingATouchingBodyPublishesNoEnd) {
    sq::ecs::Registry registry;
    sq::events::EventBus bus;
    sq::systems::PhysicsSystem physicsSystem(registry, bus);
    ContactLog log(bus);

    const sq::ecs::Entity target = spawnTarget(registry, 100.0F);
    spawnShot(registry, 95.0F, 0.0F);

    stepFor(physicsSystem, 2);
    ASSERT_EQ(log.begins.size(), 1U);

    registry.killEntity(target);
    stepFor(physicsSystem, 2);

    EXPECT_TRUE(log.ends.empty());
}

TEST(PhysicsSystemContacts, RemovingTheColliderPublishesEnd) {
    sq::ecs::Registry registry;
    sq::events::EventBus bus;
    sq::systems::PhysicsSystem physicsSystem(registry, bus);
    ContactLog log(bus);

    const sq::ecs::Entity target = spawnTarget(registry, 100.0F);
    spawnShot(registry, 95.0F, 0.0F);

    stepFor(physicsSystem, 2);
    ASSERT_EQ(log.begins.size(), 1U);

    registry.removeComponent<sq::ecs::Collider2D>(target);
    stepFor(physicsSystem, 1);

    EXPECT_EQ(log.ends.size(), 1U);
}

TEST(PhysicsSystemContacts, TwoCollidingBoxesDispatchExactlyOneBegin) {
    sq::ecs::Registry registry;
    sq::events::EventBus bus;
    sq::systems::PhysicsSystem physicsSystem(registry, bus);
    ContactLog log(bus);

    const sq::ecs::Entity wall = spawnBody(registry, 100.0F, sq::ecs::RigidBody2D{},
                                           sq::ecs::Collider2D{.halfWidth = 5.0F, .halfHeight = 50.0F});
    const sq::ecs::Entity box =
        spawnBody(registry, 0.0F, sq::ecs::RigidBody2D{.bodyType = sq::ecs::BodyType::Dynamic, .velocityX = 600.0F},
                  sq::ecs::Collider2D{.halfWidth = 8.0F, .halfHeight = 8.0F});

    stepFor(physicsSystem, 60);

    ASSERT_EQ(log.begins.size(), 1U);
    const sq::events::CollisionBeginEvent& begin = log.begins.front();
    EXPECT_TRUE((begin.entityA == box && begin.entityB == wall) || (begin.entityA == wall && begin.entityB == box));
    EXPECT_FALSE(begin.isSensor);
    EXPECT_TRUE(begin.hasContactPoint);
}

TEST(PhysicsSystemContacts, BeginCarriesPointAndNormal) {
    sq::ecs::Registry registry;
    sq::events::EventBus bus;
    sq::systems::PhysicsSystem physicsSystem(registry, bus);
    ContactLog log(bus);

    // Wall face at x = 95; the box hits it moving along +x.
    spawnBody(registry, 100.0F, sq::ecs::RigidBody2D{}, sq::ecs::Collider2D{.halfWidth = 5.0F, .halfHeight = 50.0F});
    const sq::ecs::Entity box =
        spawnBody(registry, 0.0F, sq::ecs::RigidBody2D{.bodyType = sq::ecs::BodyType::Dynamic, .velocityX = 600.0F},
                  sq::ecs::Collider2D{.halfWidth = 8.0F, .halfHeight = 8.0F});

    stepFor(physicsSystem, 60);

    ASSERT_EQ(log.begins.size(), 1U);
    const sq::events::CollisionBeginEvent& begin = log.begins.front();
    ASSERT_TRUE(begin.hasContactPoint);
    // The normal points from A to B: from the box to the wall (+x) when the box is A.
    const float towardsWall = begin.entityA == box ? 1.0F : -1.0F;
    EXPECT_NEAR(begin.normalX, towardsWall, 0.01F);
    EXPECT_NEAR(begin.normalY, 0.0F, 0.01F);
    EXPECT_NEAR(begin.pointX, 95.0F, 3.0F);
    EXPECT_NEAR(begin.pointY, 0.0F, 1.0F);
}

TEST(PhysicsSystemContacts, SensorBeginHasNoContactPoint) {
    sq::ecs::Registry registry;
    sq::events::EventBus bus;
    sq::systems::PhysicsSystem physicsSystem(registry, bus);
    ContactLog log(bus);

    spawnTarget(registry, 100.0F);
    spawnShot(registry, 95.0F, 0.0F);

    stepFor(physicsSystem, 2);

    ASSERT_EQ(log.begins.size(), 1U);
    EXPECT_TRUE(log.begins.front().isSensor);
    EXPECT_FALSE(log.begins.front().hasContactPoint);
}

TEST(PhysicsSystemContacts, ContactDispatchDoesNotAllocate) {
    sq::ecs::Registry registry;
    sq::events::EventBus bus;
    sq::systems::PhysicsSystem physicsSystem(registry, bus);
    std::size_t begins = 0;
    std::size_t ends = 0;
    bus.subscribe<sq::events::CollisionBeginEvent>([&](const sq::events::CollisionBeginEvent& /*event*/) { ++begins; });
    bus.subscribe<sq::events::CollisionEndEvent>([&](const sq::events::CollisionEndEvent& /*event*/) { ++ends; });

    spawnTarget(registry, 100.0F);
    const sq::ecs::Entity shot = spawnShot(registry, 0.0F, 600.0F);
    // Each cycle teleports the shot back and lets it cross the target: one begin and one end per cycle.
    const auto runCycles = [&](int cycles) {
        for (int cycle = 0; cycle < cycles; ++cycle) {
            registry.getComponents<sq::ecs::Transform>().get(shot).x = 0.0F;
            stepFor(physicsSystem, 30);
        }
    };

    const std::size_t startAllocations = g_allocations.load();
    runCycles(5);  // warm-up: Box2D pools, broad-phase buffers and contact buffers reach their size
    ASSERT_GT(g_allocations.load(), startAllocations) << "the allocation counter is not active";
    const std::size_t beginsBefore = begins;
    const std::size_t allocationsBefore = g_allocations.load();
    runCycles(10);
    const std::size_t allocations = g_allocations.load() - allocationsBefore;

    EXPECT_EQ(begins - beginsBefore, 10U);
    EXPECT_GE(ends, 10U);
    EXPECT_EQ(allocations, 0U);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
