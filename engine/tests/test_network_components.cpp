#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "net/InputQueue.hpp"
#include "net/NetworkEntityMap.hpp"
#include "net/components/InputBuffer.hpp"
#include "net/components/NetworkId.hpp"
#include "net/components/NetworkOwner.hpp"
#include "net/components/Replicated.hpp"

namespace {

sq::ecs::NetInput input(const std::uint32_t sequence) {
    return sq::ecs::NetInput{.sequence = sequence, .held = {}, .axes = {}};
}

/** @brief Pops every pending input and returns the last processed sequence. */
std::uint32_t drain(sq::ecs::InputBuffer& buffer) {
    while (sq::net::popInput(buffer)) {
    }
    return buffer.lastProcessed;
}

}  // namespace

TEST(NetworkComponents, DefaultValues) {
    EXPECT_EQ(sq::ecs::NetworkId{}.value, 0U);
    EXPECT_EQ(sq::ecs::NetworkOwner{}.client, 0U);
    EXPECT_EQ(sq::ecs::Replicated{}.kind, 0U);
    const sq::ecs::InputBuffer buffer;
    EXPECT_TRUE(buffer.pending.empty());
    EXPECT_EQ(buffer.lastProcessed, 0U);
    EXPECT_EQ(buffer.capacity, sq::ecs::DEFAULT_MAX_PENDING_INPUTS);
}

TEST(NetworkComponents, WorkAsEcsComponents) {
    sq::ecs::Registry registry;
    const sq::ecs::Entity entity = registry.spawnEntity();
    registry.emplaceComponent<sq::ecs::NetworkId>(entity, 7U);
    registry.emplaceComponent<sq::ecs::NetworkOwner>(entity, 3U);
    registry.emplaceComponent<sq::ecs::Replicated>(entity, static_cast<std::uint32_t>(2));
    registry.emplaceComponent<sq::ecs::InputBuffer>(entity);

    int visited = 0;
    registry.runSystem<sq::ecs::NetworkId, sq::ecs::NetworkOwner, sq::ecs::Replicated, sq::ecs::InputBuffer>(
        [&](sq::ecs::Entity e, const sq::ecs::NetworkId& id, const sq::ecs::NetworkOwner& owner,
            const sq::ecs::Replicated& replicated, const sq::ecs::InputBuffer&) {
            EXPECT_EQ(e, entity);
            EXPECT_EQ(id.value, 7U);
            EXPECT_EQ(owner.client, 3U);
            EXPECT_EQ(replicated.kind, 2U);
            ++visited;
        });
    EXPECT_EQ(visited, 1);
}

TEST(InputQueue, PopsInSequenceOrder) {
    sq::ecs::InputBuffer buffer;
    EXPECT_TRUE(sq::net::pushInput(buffer, input(3)));
    EXPECT_TRUE(sq::net::pushInput(buffer, input(1)));
    EXPECT_TRUE(sq::net::pushInput(buffer, input(2)));

    for (std::uint32_t expected = 1; expected <= 3; ++expected) {
        const auto popped = sq::net::popInput(buffer);
        ASSERT_TRUE(popped.has_value());
        EXPECT_EQ(popped->sequence, expected);
        EXPECT_EQ(buffer.lastProcessed, expected);
    }
    EXPECT_FALSE(sq::net::popInput(buffer).has_value());
}

TEST(InputQueue, RejectsDuplicatesAndAlreadyProcessed) {
    sq::ecs::InputBuffer buffer;
    EXPECT_TRUE(sq::net::pushInput(buffer, input(5)));
    EXPECT_FALSE(sq::net::pushInput(buffer, input(5)));
    (void)sq::net::popInput(buffer);

    EXPECT_FALSE(sq::net::pushInput(buffer, input(5)));
    EXPECT_FALSE(sq::net::pushInput(buffer, input(4)));
    EXPECT_TRUE(buffer.pending.empty());
}

TEST(InputQueue, OverflowDropsTheOldest) {
    sq::ecs::InputBuffer buffer{.capacity = 3};
    for (std::uint32_t seq = 1; seq <= 5; ++seq) {
        sq::net::pushInput(buffer, input(seq));
    }
    ASSERT_EQ(buffer.pending.size(), 3U);
    EXPECT_EQ(buffer.pending.front().sequence, 3U);
    EXPECT_EQ(drain(buffer), 5U);
}

TEST(InputQueue, FullQueueRejectsInputOlderThanAllPending) {
    sq::ecs::InputBuffer buffer{.capacity = 3};
    for (std::uint32_t seq = 5; seq <= 7; ++seq) {
        ASSERT_TRUE(sq::net::pushInput(buffer, input(seq)));
    }

    EXPECT_FALSE(sq::net::pushInput(buffer, input(4)));
    ASSERT_EQ(buffer.pending.size(), 3U);
    EXPECT_EQ(buffer.pending.front().sequence, 5U);
}

TEST(InputQueue, FullQueueAcceptsInputNewerThanTheOldest) {
    sq::ecs::InputBuffer buffer{.capacity = 3};
    ASSERT_TRUE(sq::net::pushInput(buffer, input(5)));
    ASSERT_TRUE(sq::net::pushInput(buffer, input(7)));
    ASSERT_TRUE(sq::net::pushInput(buffer, input(8)));

    EXPECT_TRUE(sq::net::pushInput(buffer, input(6)));
    ASSERT_EQ(buffer.pending.size(), 3U);
    EXPECT_EQ(buffer.pending.front().sequence, 6U);
}

TEST(InputQueue, KeepsTheInputPayload) {
    sq::ecs::InputBuffer buffer;
    ASSERT_TRUE(sq::net::pushInput(
        buffer, sq::ecs::NetInput{.sequence = 1, .held = {42}, .axes = {{.action = 7, .value = 0.5F}}}));

    const auto popped = sq::net::popInput(buffer);
    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(popped->held, std::vector<std::uint32_t>{42});
    ASSERT_EQ(popped->axes.size(), 1U);
    EXPECT_EQ(popped->axes[0].action, 7U);
    EXPECT_FLOAT_EQ(popped->axes[0].value, 0.5F);
}

TEST(InputQueue, ZeroCapacityKeepsNothing) {
    sq::ecs::InputBuffer buffer{.capacity = 0};
    EXPECT_FALSE(sq::net::pushInput(buffer, input(1)));
    EXPECT_TRUE(buffer.pending.empty());
}

TEST(NetworkEntityMap, AssignGivesUniqueIdsStartingAtOne) {
    sq::net::NetworkEntityMap map;
    const auto first = map.assign(10);
    const auto second = map.assign(20);

    EXPECT_EQ(first, 1U);
    EXPECT_EQ(second, 2U);
    EXPECT_EQ(map.assign(10), first);  // already mapped: same id
    EXPECT_EQ(map.entityOf(second), std::optional<sq::ecs::Entity>{20});
    EXPECT_EQ(map.idOf(10), std::optional<std::uint32_t>{first});
    EXPECT_EQ(map.size(), 2U);
}

TEST(NetworkEntityMap, IdsAreNotReusedAfterUnbind) {
    sq::net::NetworkEntityMap map;
    const auto id = map.assign(10);
    map.unbindEntity(10);

    EXPECT_FALSE(map.entityOf(id).has_value());
    EXPECT_NE(map.assign(10), id);
}

TEST(NetworkEntityMap, BindReplacesPreviousMappings) {
    sq::net::NetworkEntityMap map;
    EXPECT_FALSE(map.bind(0, 1));
    EXPECT_TRUE(map.bind(42, 1));
    EXPECT_TRUE(map.bind(42, 2));  // id moves to another entity
    EXPECT_FALSE(map.idOf(1).has_value());
    EXPECT_EQ(map.entityOf(42), std::optional<sq::ecs::Entity>{2});

    EXPECT_TRUE(map.bind(43, 2));  // entity gets another id
    EXPECT_FALSE(map.entityOf(42).has_value());
    EXPECT_EQ(map.size(), 1U);

    map.unbind(43);
    EXPECT_EQ(map.size(), 0U);
}

TEST(NetworkEntityMap, WorksAsRegistryResource) {
    sq::ecs::Registry registry;
    auto& map = registry.emplaceResource<sq::net::NetworkEntityMap>();
    const sq::ecs::Entity entity = registry.spawnEntity();
    registry.emplaceComponent<sq::ecs::NetworkId>(entity, map.assign(entity));

    const auto& id = registry.getComponents<sq::ecs::NetworkId>().get(entity);
    EXPECT_EQ(map.entityOf(id.value), std::optional<sq::ecs::Entity>{entity});
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
