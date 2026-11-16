#include <gtest/gtest.h>

#include <cstddef>

#include "core/services/InputCodes.hpp"
#include "input/ActionMapping.hpp"
#include "input/RTypeBindings.hpp"

TEST(RTypeBindings, SetupDefaultBindings) {
    input::ActionMapping mapping;
    client::input::setupDefaultBindings(mapping);

    // Spatial cluster keys: Key::A (physical Left) and Key::W (physical Up)
    EXPECT_TRUE(mapping.hasKey("move_x", engine::Key::A));
    EXPECT_TRUE(mapping.hasKey("move_x", engine::Key::D));
    EXPECT_TRUE(mapping.hasKey("move_x", engine::Key::Left));
    EXPECT_TRUE(mapping.hasKey("move_x", engine::Key::Right));
    // Physical 'A' on AZERTY (Key::Q) and physical 'W' on AZERTY (Key::Z) must NOT move the ship
    EXPECT_FALSE(mapping.hasKey("move_x", engine::Key::Q));

    EXPECT_TRUE(mapping.hasKey("move_y", engine::Key::W));
    EXPECT_TRUE(mapping.hasKey("move_y", engine::Key::S));
    EXPECT_TRUE(mapping.hasKey("move_y", engine::Key::Up));
    EXPECT_TRUE(mapping.hasKey("move_y", engine::Key::Down));
    EXPECT_FALSE(mapping.hasKey("move_y", engine::Key::Z));

    EXPECT_TRUE(mapping.hasKey("shoot", engine::Key::Space));
    EXPECT_TRUE(mapping.hasKey("charge_weapon", engine::Key::LeftShift));
}

TEST(RTypeBindings, CustomKeysReplaceTheMainKeysAndKeepTheSecondaryOnes) {
    input::ActionMapping mapping;
    client::input::ControlKeys keys = client::input::defaultControlKeys();
    client::input::assignKey(keys, client::input::Control::Shoot, engine::Key::F);
    client::input::setupBindings(mapping, keys);

    EXPECT_TRUE(mapping.hasKey("shoot", engine::Key::F));
    EXPECT_TRUE(mapping.hasKey("shoot", engine::Key::Enter));
    EXPECT_FALSE(mapping.hasKey("shoot", engine::Key::Space));
    EXPECT_TRUE(mapping.hasKey("move_x", engine::Key::A));
}

TEST(RTypeBindings, AssigningAKeyInUseSwapsTheTwoControls) {
    client::input::ControlKeys keys = client::input::defaultControlKeys();
    client::input::assignKey(keys, client::input::Control::Shoot, engine::Key::W);

    EXPECT_EQ(keys.at(static_cast<std::size_t>(client::input::Control::Shoot)), engine::Key::W);
    EXPECT_EQ(keys.at(static_cast<std::size_t>(client::input::Control::MoveUp)), engine::Key::Space);
}

TEST(RTypeBindings, ASecondaryKeyUsedAsAMainKeyIsDropped) {
    input::ActionMapping mapping;
    client::input::ControlKeys keys = client::input::defaultControlKeys();
    client::input::assignKey(keys, client::input::Control::Shoot, engine::Key::Up);
    client::input::setupBindings(mapping, keys);

    EXPECT_TRUE(mapping.hasKey("shoot", engine::Key::Up));
    EXPECT_FALSE(mapping.hasKey("move_y", engine::Key::Up));
    EXPECT_TRUE(mapping.hasKey("move_y", engine::Key::Down));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
