#include <gtest/gtest.h>

#include <cstddef>

#include "input/ActionMapping.hpp"
#include "input/RTypeBindings.hpp"
#include "render/api/InputCodes.hpp"

TEST(RTypeBindings, SetupDefaultBindings) {
    sq::input::ActionMapping mapping;
    client::input::setupDefaultBindings(mapping);

    // Spatial cluster keys: Key::A (physical Left) and Key::W (physical Up)
    EXPECT_TRUE(mapping.hasKey("move_x", sq::engine::Key::A));
    EXPECT_TRUE(mapping.hasKey("move_x", sq::engine::Key::D));
    EXPECT_TRUE(mapping.hasKey("move_x", sq::engine::Key::Left));
    EXPECT_TRUE(mapping.hasKey("move_x", sq::engine::Key::Right));
    // Physical 'A' on AZERTY (Key::Q) and physical 'W' on AZERTY (Key::Z) must NOT move the ship
    EXPECT_FALSE(mapping.hasKey("move_x", sq::engine::Key::Q));

    EXPECT_TRUE(mapping.hasKey("move_y", sq::engine::Key::W));
    EXPECT_TRUE(mapping.hasKey("move_y", sq::engine::Key::S));
    EXPECT_TRUE(mapping.hasKey("move_y", sq::engine::Key::Up));
    EXPECT_TRUE(mapping.hasKey("move_y", sq::engine::Key::Down));
    EXPECT_FALSE(mapping.hasKey("move_y", sq::engine::Key::Z));

    EXPECT_TRUE(mapping.hasKey("shoot", sq::engine::Key::Space));
    EXPECT_TRUE(mapping.hasKey("charge_weapon", sq::engine::Key::LeftShift));
}

TEST(RTypeBindings, CustomKeysReplaceTheMainKeysAndKeepTheSecondaryOnes) {
    sq::input::ActionMapping mapping;
    client::input::ControlKeys keys = client::input::defaultControlKeys();
    client::input::assignKey(keys, client::input::Control::Shoot, sq::engine::Key::F);
    client::input::setupBindings(mapping, keys);

    EXPECT_TRUE(mapping.hasKey("shoot", sq::engine::Key::F));
    EXPECT_TRUE(mapping.hasKey("shoot", sq::engine::Key::Enter));
    EXPECT_FALSE(mapping.hasKey("shoot", sq::engine::Key::Space));
    EXPECT_TRUE(mapping.hasKey("move_x", sq::engine::Key::A));
}

TEST(RTypeBindings, AssigningAKeyInUseSwapsTheTwoControls) {
    client::input::ControlKeys keys = client::input::defaultControlKeys();
    client::input::assignKey(keys, client::input::Control::Shoot, sq::engine::Key::W);

    EXPECT_EQ(keys.at(static_cast<std::size_t>(client::input::Control::Shoot)), sq::engine::Key::W);
    EXPECT_EQ(keys.at(static_cast<std::size_t>(client::input::Control::MoveUp)), sq::engine::Key::Space);
}

TEST(RTypeBindings, ASecondaryKeyUsedAsAMainKeyIsDropped) {
    sq::input::ActionMapping mapping;
    client::input::ControlKeys keys = client::input::defaultControlKeys();
    client::input::assignKey(keys, client::input::Control::Shoot, sq::engine::Key::Up);
    client::input::setupBindings(mapping, keys);

    EXPECT_TRUE(mapping.hasKey("shoot", sq::engine::Key::Up));
    EXPECT_FALSE(mapping.hasKey("move_y", sq::engine::Key::Up));
    EXPECT_TRUE(mapping.hasKey("move_y", sq::engine::Key::Down));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
