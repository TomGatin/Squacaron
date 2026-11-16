#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <variant>
#include <vector>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "game/ReplicationCodec.hpp"
#include "game/ShipMovement.hpp"
#include "net/INetworkManager.hpp"
#include "net/NetworkConfig.hpp"
#include "net/NetworkEntityMap.hpp"
#include "net/SnapshotApplier.hpp"
#include "net/components/InputBuffer.hpp"
#include "net/components/Interpolated.hpp"
#include "net/components/Replicated.hpp"
#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"

namespace {

using rtype::game::ACTION_MOVE_X;
using rtype::game::ACTION_MOVE_Y;
using rtype::game::applyShipInput;
using rtype::game::EntityNetState;
using rtype::game::ReplicationCodec;
using rtype::game::SHIP_SPEED;
using sq::net::Bytes;
namespace protocol = rtype::protocol;

constexpr float DT = 0.01F;
constexpr std::uint32_t KIND = protocol::makeEntityKind("alien_scout");

sq::ecs::NetInput axes(std::vector<sq::ecs::NetAxis> values) {
    return sq::ecs::NetInput{.sequence = 1, .held = {}, .axes = std::move(values)};
}

/** @brief A ship far from the borders of the play area. */
constexpr sq::ecs::Transform CENTER{.x = 500.0F, .y = 300.0F};

/** @brief Decodes a snapshot written by the codec. */
protocol::WorldState decodeWorld(const Bytes& bytes) {
    const auto message = protocol::decode(bytes);
    EXPECT_TRUE(message.has_value());
    return std::get<protocol::WorldState>(message.value());
}

}  // namespace

// --- Movement --------------------------------------------------------------------------------------------------------

TEST(ShipMovement, MovesAlongTheAxesOfTheInput) {
    sq::ecs::Transform ship = CENTER;
    applyShipInput(ship, axes({{.action = ACTION_MOVE_X, .value = 1.0F}, {.action = ACTION_MOVE_Y, .value = -0.5F}}),
                   DT);

    EXPECT_GT(ship.x, CENTER.x);
    EXPECT_LT(ship.y, CENTER.y);
}

TEST(ShipMovement, StraightLineUsesTheFullSpeed) {
    sq::ecs::Transform ship = CENTER;
    applyShipInput(ship, axes({{.action = ACTION_MOVE_X, .value = 1.0F}}), DT);

    EXPECT_FLOAT_EQ(ship.x, CENTER.x + (SHIP_SPEED * DT));
    EXPECT_FLOAT_EQ(ship.y, CENTER.y);
}

TEST(ShipMovement, DiagonalIsNotFaster) {
    sq::ecs::Transform ship = CENTER;
    applyShipInput(ship, axes({{.action = ACTION_MOVE_X, .value = 1.0F}, {.action = ACTION_MOVE_Y, .value = 1.0F}}),
                   DT);

    const float dx = ship.x - CENTER.x;
    const float dy = ship.y - CENTER.y;
    EXPECT_NEAR((dx * dx) + (dy * dy), (SHIP_SPEED * DT) * (SHIP_SPEED * DT), 1e-2F);
}

TEST(ShipMovement, StaysInThePlayArea) {
    sq::ecs::Transform ship{.x = rtype::game::SHIP_MAX_X, .y = rtype::game::SHIP_MIN_Y};
    applyShipInput(ship, axes({{.action = ACTION_MOVE_X, .value = 1.0F}, {.action = ACTION_MOVE_Y, .value = -1.0F}}),
                   1.0F);

    EXPECT_FLOAT_EQ(ship.x, rtype::game::SHIP_MAX_X);
    EXPECT_FLOAT_EQ(ship.y, rtype::game::SHIP_MIN_Y);
}

TEST(ShipMovement, UntrustedAxisCannotMakeTheShipFaster) {
    sq::ecs::Transform ship = CENTER;
    applyShipInput(ship, axes({{.action = ACTION_MOVE_X, .value = 1000.0F}}), DT);
    EXPECT_FLOAT_EQ(ship.x, CENTER.x + (SHIP_SPEED * DT));

    ship = CENTER;
    applyShipInput(ship, axes({{.action = ACTION_MOVE_X, .value = std::numeric_limits<float>::quiet_NaN()}}), DT);
    EXPECT_FLOAT_EQ(ship.x, CENTER.x);
}

TEST(ShipMovement, IgnoresOtherActions) {
    sq::ecs::Transform ship = CENTER;
    applyShipInput(ship, axes({{.action = 0x1234, .value = 1.0F}}), DT);

    EXPECT_FLOAT_EQ(ship.x, CENTER.x);
    EXPECT_FLOAT_EQ(ship.y, CENTER.y);
}

// --- Inputs ----------------------------------------------------------------------------------------------------------

TEST(ReplicationCodec, InputRoundTrip) {
    ReplicationCodec codec;
    const sq::ecs::NetInput input{.sequence = 42, .held = {5, 6}, .axes = {{.action = ACTION_MOVE_X, .value = -1.0F}}};

    const auto decoded = codec.decodeInput(codec.encodeInput(input));

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->sequence, 42U);
    EXPECT_EQ(decoded->held, (std::vector<std::uint32_t>{5, 6}));
    ASSERT_EQ(decoded->axes.size(), 1U);
    EXPECT_EQ(decoded->axes[0].action, ACTION_MOVE_X);
    EXPECT_FLOAT_EQ(decoded->axes[0].value, -1.0F);
}

TEST(ReplicationCodec, RefusesPayloadsThatAreNotInputs) {
    ReplicationCodec codec;

    EXPECT_FALSE(codec.decodeInput({}).has_value());
    EXPECT_FALSE(codec.decodeInput({0xFF, 0x00, 0x13}).has_value());
    EXPECT_FALSE(codec.decodeInput(protocol::encode(protocol::Leave{})).has_value());
}

// --- Snapshots -------------------------------------------------------------------------------------------------------

TEST(ReplicationCodec, WritesAnEntityAndTheAcknowledgementOfEachPeer) {
    ReplicationCodec codec;
    sq::ecs::Registry registry;
    const sq::ecs::Entity entity = registry.spawnEntity();
    registry.addComponent(entity, sq::ecs::Transform{.x = 1.5F, .y = 2.5F});
    registry.addComponent(entity, EntityNetState{.subtype = 3, .health = 9, .flags = rtype::game::FLAG_SHOOTING});

    codec.beginSnapshot(11);
    codec.writeEntity(registry, entity, 4, sq::ecs::Replicated{KIND});

    const auto first = decodeWorld(codec.encodeSnapshot(100).at(0));
    const auto second = decodeWorld(codec.encodeSnapshot(200).at(0));
    EXPECT_EQ(first.tick, 11U);
    EXPECT_EQ(first.lastProcessedInput, 100U);
    EXPECT_EQ(second.lastProcessedInput, 200U);
    EXPECT_EQ(first.entities, second.entities);
    ASSERT_EQ(first.entities.size(), 1U);
    EXPECT_EQ(first.entities[0], (protocol::EntityState{.entityId = 4,
                                                        .kind = KIND,
                                                        .subtype = 3,
                                                        .x = 1.5F,
                                                        .y = 2.5F,
                                                        .health = 9,
                                                        .flags = rtype::game::FLAG_SHOOTING}));
}

TEST(ReplicationCodec, BeginSnapshotForgetsThePreviousEntities) {
    ReplicationCodec codec;
    sq::ecs::Registry registry;
    const sq::ecs::Entity entity = registry.spawnEntity();

    codec.beginSnapshot(1);
    codec.writeEntity(registry, entity, 4, sq::ecs::Replicated{KIND});
    codec.beginSnapshot(2);

    EXPECT_TRUE(decodeWorld(codec.encodeSnapshot(0).at(0)).entities.empty());
}

TEST(ReplicationCodec, SkipsAnEntityWithANonFinitePosition) {
    ReplicationCodec codec;
    sq::ecs::Registry registry;
    const sq::ecs::Entity broken = registry.spawnEntity();
    registry.addComponent(broken, sq::ecs::Transform{.x = std::numeric_limits<float>::infinity()});
    const sq::ecs::Entity valid = registry.spawnEntity();

    codec.beginSnapshot(1);
    codec.writeEntity(registry, broken, 1, sq::ecs::Replicated{KIND});
    codec.writeEntity(registry, valid, 2, sq::ecs::Replicated{KIND});

    const auto world = decodeWorld(codec.encodeSnapshot(0).at(0));
    ASSERT_EQ(world.entities.size(), 1U) << "otherwise clients would reject the whole snapshot";
    EXPECT_EQ(world.entities[0].entityId, 2U);
}

TEST(ReplicationCodec, LargeSnapshotIsSplitIntoDatagrams) {
    ReplicationCodec codec;
    sq::ecs::Registry registry;
    const sq::ecs::Entity entity = registry.spawnEntity();
    constexpr std::uint32_t ENTITIES = 200;

    codec.beginSnapshot(7);
    for (std::uint32_t id = 1; id <= ENTITIES; ++id) {
        codec.writeEntity(registry, entity, id, sq::ecs::Replicated{KIND});
    }
    const auto parts = codec.encodeSnapshot(4);

    ASSERT_EQ(parts.size(), 6U) << "36 entities per part";
    std::vector<std::uint32_t> ids;
    for (std::size_t index = 0; index < parts.size(); ++index) {
        EXPECT_LE(parts.at(index).size(), sq::net::MAX_UNRELIABLE_PAYLOAD);
        const auto header = codec.decodeSnapshot(parts.at(index));
        ASSERT_TRUE(header.has_value());
        EXPECT_EQ(header->tick, 7U);
        EXPECT_EQ(header->lastProcessedInput, 4U);
        EXPECT_EQ(header->partIndex, index);
        EXPECT_EQ(header->partCount, parts.size());
        for (const auto& state : decodeWorld(parts.at(index)).entities) {
            ids.push_back(state.entityId);
        }
    }
    ASSERT_EQ(ids.size(), ENTITIES) << "every entity is in exactly one part";
    EXPECT_TRUE(std::ranges::is_sorted(ids));
}

TEST(ReplicationCodec, EmptyWorldIsOnePart) {
    ReplicationCodec codec;

    codec.beginSnapshot(1);
    const auto parts = codec.encodeSnapshot(0);

    ASSERT_EQ(parts.size(), 1U) << "the client must learn that its entities are gone";
    EXPECT_TRUE(decodeWorld(parts.front()).entities.empty());
}

TEST(ReplicationCodec, SnapshotBeyondTheLastPartIsNotEncoded) {
    ReplicationCodec codec;
    sq::ecs::Registry registry;
    const sq::ecs::Entity entity = registry.spawnEntity();

    codec.beginSnapshot(1);
    const auto tooMany = (protocol::MAX_ENTITIES_PER_PART * protocol::MAX_SNAPSHOT_PARTS) + 1;
    for (std::uint32_t id = 1; id <= tooMany; ++id) {
        codec.writeEntity(registry, entity, id, sq::ecs::Replicated{KIND});
    }

    EXPECT_TRUE(codec.encodeSnapshot(0).empty());
}

TEST(ReplicationCodec, RefusesPayloadsThatAreNotSnapshots) {
    ReplicationCodec codec;

    EXPECT_FALSE(codec.decodeSnapshot({}).has_value());
    EXPECT_FALSE(codec.decodeSnapshot({0xFF, 0x00, 0x13}).has_value());
    EXPECT_FALSE(codec.decodeSnapshot(protocol::encode(protocol::Hello{})).has_value());
}

class CodecApplyTest : public ::testing::Test {
  protected:
    /** @brief Runs `state` through decode and apply, as the client does. */
    sq::ecs::Entity apply(const protocol::EntityState& state) {
        const auto header = codec.decodeSnapshot(
            protocol::encode(protocol::WorldState{.tick = 5, .lastProcessedInput = 6, .entities = {state}}));
        EXPECT_TRUE(header.has_value());
        EXPECT_EQ(header->tick, 5U);
        EXPECT_EQ(header->lastProcessedInput, 6U);
        sq::net::SnapshotApplier applier(registry, map, DT);
        codec.applySnapshot(applier);
        return map.entityOf(state.entityId).value();
    }

    ReplicationCodec codec;
    sq::ecs::Registry registry;
    sq::net::NetworkEntityMap map;
};

TEST_F(CodecApplyTest, CreatesTheEntityWithItsState) {
    const sq::ecs::Entity entity =
        apply({.entityId = 4, .kind = KIND, .subtype = 2, .x = 7.0F, .y = 8.0F, .health = 3, .flags = 1});

    EXPECT_EQ(registry.getComponents<sq::ecs::Replicated>().get(entity).kind, KIND);
    EXPECT_FLOAT_EQ(registry.getComponents<sq::ecs::Transform>().get(entity).x, 7.0F);
    EXPECT_FLOAT_EQ(registry.getComponents<sq::ecs::Transform>().get(entity).y, 8.0F);
    EXPECT_EQ(registry.getComponents<EntityNetState>().get(entity).subtype, 2);
    EXPECT_EQ(registry.getComponents<EntityNetState>().get(entity).health, 3);
    EXPECT_EQ(registry.getComponents<EntityNetState>().get(entity).flags, 1);
}

TEST_F(CodecApplyTest, KeepsTheFieldsOfTheTransformNotOnTheWire) {
    const sq::ecs::Entity entity = apply({.entityId = 4, .kind = KIND, .x = 1.0F, .y = 1.0F});
    registry.getComponents<sq::ecs::Transform>().get(entity).scaleX = 2.0F;
    registry.getComponents<sq::ecs::Transform>().get(entity).z = 3.0F;

    apply({.entityId = 4, .kind = KIND, .x = 9.0F, .y = 1.0F});

    const auto& transform = registry.getComponents<sq::ecs::Transform>().get(entity);
    EXPECT_FLOAT_EQ(transform.x, 9.0F);
    EXPECT_FLOAT_EQ(transform.scaleX, 2.0F) << "a client sprite keeps its size";
    EXPECT_FLOAT_EQ(transform.z, 3.0F);
}

TEST_F(CodecApplyTest, GoesThroughInterpolationWhenTheEntityUsesIt) {
    const sq::ecs::Entity entity = apply({.entityId = 4, .kind = KIND, .x = 1.0F, .y = 1.0F});
    registry.addComponent(
        entity, sq::ecs::Interpolated{.from = {}, .to = {}, .elapsed = 0.0F, .duration = 0.0F, .initialized = true});

    apply({.entityId = 4, .kind = KIND, .x = 9.0F, .y = 1.0F});

    EXPECT_FLOAT_EQ(registry.getComponents<sq::ecs::Interpolated>().get(entity).to.x, 9.0F);
    EXPECT_FLOAT_EQ(registry.getComponents<sq::ecs::Transform>().get(entity).x, 1.0F)
        << "the system moves it, frame by frame";
}

TEST_F(CodecApplyTest, IgnoresAnEntityWithoutId) {
    ASSERT_TRUE(codec
                    .decodeSnapshot(protocol::encode(protocol::WorldState{
                        .tick = 1, .lastProcessedInput = 0, .entities = {{.entityId = 0, .kind = KIND}}}))
                    .has_value());
    sq::net::SnapshotApplier applier(registry, map, DT);

    codec.applySnapshot(applier);

    EXPECT_TRUE(applier.spawned().empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
