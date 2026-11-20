#include <gtest/gtest.h>

#include <cstdint>
#include <tuple>

#include "net/INetworkManager.hpp"
#include "net/asio/EventQueue.hpp"
#include "net/asio/TransportCounters.hpp"

namespace {

using ecs::net::Bytes;
using ecs::net::Channel;
using ecs::net::EventType;
using ecs::net::NetworkEvent;
using ecs::net::PeerId;

NetworkEvent data(const PeerId peer, const std::uint8_t value) {
    return {.type = EventType::Data, .peer = peer, .data = Bytes{value}, .channel = Channel::Unreliable};
}

}  // namespace

TEST(EventQueue, QueueFilledPastTheCapStaysAtTheCap) {
    ecs::net::detail::TransportCounters counters;
    ecs::net::detail::EventQueue queue(4, counters);

    for (std::uint8_t i = 0; i < 100; ++i) {
        std::ignore = queue.push(data(1, i));
    }

    EXPECT_EQ(queue.size(), 4U);
    EXPECT_EQ(counters.read().eventsDropped, 96U);
    const auto events = queue.popAll();
    ASSERT_EQ(events.size(), 4U);
    EXPECT_EQ(events.back().data, Bytes{3}) << "the oldest messages are kept";
}

TEST(EventQueue, CapIsPerPeerAndPerPoll) {
    ecs::net::detail::TransportCounters counters;
    ecs::net::detail::EventQueue queue(2, counters);

    EXPECT_TRUE(queue.push(data(1, 0)));
    EXPECT_TRUE(queue.push(data(1, 1)));
    EXPECT_FALSE(queue.push(data(1, 2)));
    EXPECT_TRUE(queue.push(data(2, 0))) << "another peer has its own budget";

    EXPECT_EQ(queue.popAll().size(), 3U);
    EXPECT_TRUE(queue.push(data(1, 3))) << "polling frees the budget";
    EXPECT_EQ(counters.read().eventsDropped, 1U);
}

TEST(EventQueue, ConnectionEventsAreNeverDropped) {
    ecs::net::detail::TransportCounters counters;
    ecs::net::detail::EventQueue queue(1, counters);

    std::ignore = queue.push(data(1, 0));
    std::ignore = queue.push(data(1, 1));
    EXPECT_TRUE(queue.push({.type = EventType::Disconnected, .peer = 1, .data = {}, .channel = Channel::Reliable}));
    EXPECT_TRUE(queue.push({.type = EventType::Connected, .peer = 2, .data = {}, .channel = Channel::Reliable}));

    EXPECT_EQ(queue.popAll().size(), 3U);
}
