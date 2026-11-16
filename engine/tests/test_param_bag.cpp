#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>

#include "ecs/ParamBag.hpp"
#include "prefabs/PrefabParams.hpp"
#include "timeline/TimelineEvent.hpp"

TEST(ParamBag, ReadsEachStoredType) {
    sq::ecs::ParamBag const params{
        {"count", std::int64_t{3}}, {"speed", 2.5}, {"name", std::string("scout")}, {"on", true}};

    EXPECT_EQ(params.get<std::int64_t>("count", 0), 3);
    EXPECT_DOUBLE_EQ(params.get("speed", 0.0), 2.5);
    EXPECT_EQ(params.get<std::string>("name", ""), "scout");
    EXPECT_TRUE(params.get("on", false));
}

TEST(ParamBag, NumbersConvertButOtherTypesDoNot) {
    sq::ecs::ParamBag const params{{"speed", 10}, {"name", std::string("boss")}, {"on", true}};

    EXPECT_FLOAT_EQ(params.get("speed", 0.0F), 10.0F);
    EXPECT_EQ(params.find<int>("name"), std::nullopt);
    EXPECT_EQ(params.find<double>("on"), std::nullopt);
    EXPECT_EQ(params.find<bool>("speed"), std::nullopt);
}

TEST(ParamBag, MissingKeysFallBack) {
    sq::ecs::ParamBag const params;

    EXPECT_FALSE(params.has("x"));
    EXPECT_EQ(params.find<float>("x"), std::nullopt);
    EXPECT_FLOAT_EQ(params.get("x", 4.0F), 4.0F);
}

TEST(ParamBag, SetReplacesAndMergeOverrides) {
    sq::ecs::ParamBag params{{"x", 1.0}, {"y", 2.0}};
    params.set("x", 3.0).set("z", 4.0);
    params.merge(sq::ecs::ParamBag{{"y", 5.0}});

    EXPECT_DOUBLE_EQ(params.get("x", 0.0), 3.0);
    EXPECT_DOUBLE_EQ(params.get("y", 0.0), 5.0);
    EXPECT_DOUBLE_EQ(params.get("z", 0.0), 4.0);
}

TEST(ParamBag, IsSharedByPrefabsAndTimelineEvents) {
    static_assert(std::is_same_v<sq::prefabs::PrefabParams, sq::ecs::ParamBag>);
    static_assert(std::is_same_v<decltype(sq::timeline::TimelineEvent::params), sq::ecs::ParamBag>);

    sq::timeline::TimelineEvent const event{
        .position = 0.0F, .type = "spawn", .params = {{"prefab", std::string("scout")}}};
    sq::prefabs::PrefabParams const params = event.params;
    EXPECT_EQ(params.get<std::string>("prefab", ""), "scout");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
