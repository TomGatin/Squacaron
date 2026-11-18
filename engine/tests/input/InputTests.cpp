#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <memory>
#include <string_view>

#include "core/Engine.hpp"
#include "core/IPlugin.hpp"
#include "core/Mediator.hpp"
#include "core/PluginBuilder.hpp"
#include "core/PluginGraph.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "input/ActionId.hpp"
#include "input/ActionMapping.hpp"
#include "input/InputActionEvent.hpp"
#include "input/InputConstants.hpp"
#include "input/InputPlugin.hpp"
#include "input/InputState.hpp"
#include "input/KeyNames.hpp"
#include "input/KeyboardLayout.hpp"
#include "render/api/IWindowService.hpp"
#include "render/api/InputCodes.hpp"
#include "systems/input/InputSystem.hpp"

namespace {

class FakeWindow final : public sq::engine::IWindowService {
  public:
    explicit FakeWindow(sq::engine::Key heldKey = sq::engine::Key::Unknown) : _heldKey(heldKey) {}
    void holdButton(sq::engine::GamepadButton button) { _heldButton = button; }
    void tiltAxis(sq::engine::GamepadAxis axis, float value) {
        _tiltedAxis = axis;
        _axisValue = value;
    }
    void holdMouseButton(sq::engine::MouseButton button) { _heldMouseButton = button; }
    void moveMouse(sq::engine::MousePosition position, float wheel) {
        _mousePosition = position;
        _mouseWheel = wheel;
    }
    [[nodiscard]] bool shouldClose() const noexcept override { return false; }
    void beginFrame() override {}
    void endFrame() override {}
    [[nodiscard]] int width() const noexcept override { return 0; }
    [[nodiscard]] int height() const noexcept override { return 0; }
    [[nodiscard]] bool isKeyDown(sq::engine::Key key) const noexcept override { return key == _heldKey; }
    [[nodiscard]] bool isGamepadAvailable(int /*gamepad*/) const noexcept override { return true; }
    [[nodiscard]] bool isGamepadButtonDown(int /*gamepad*/, sq::engine::GamepadButton button) const noexcept override {
        return button == _heldButton;
    }
    [[nodiscard]] float gamepadAxis(int /*gamepad*/, sq::engine::GamepadAxis axis) const noexcept override {
        return axis == _tiltedAxis ? _axisValue : 0.0F;
    }
    [[nodiscard]] bool isMouseButtonDown(sq::engine::MouseButton button) const noexcept override {
        return button == _heldMouseButton;
    }
    [[nodiscard]] bool isMouseButtonPressed(sq::engine::MouseButton /*button*/) const noexcept override {
        return false;
    }
    [[nodiscard]] bool isMouseButtonReleased(sq::engine::MouseButton /*button*/) const noexcept override {
        return false;
    }
    [[nodiscard]] sq::engine::MousePosition mousePosition() const noexcept override { return _mousePosition; }
    [[nodiscard]] float mouseWheelDelta() const noexcept override { return _mouseWheel; }
    [[nodiscard]] char32_t nextTypedCharacter() noexcept override { return U'\0'; }

  private:
    sq::engine::Key _heldKey;
    sq::engine::GamepadButton _heldButton = sq::engine::GamepadButton::Unknown;
    sq::engine::GamepadAxis _tiltedAxis = sq::engine::GamepadAxis::None;
    float _axisValue = 0.0F;
    sq::engine::MouseButton _heldMouseButton = sq::engine::MouseButton::Unknown;
    sq::engine::MousePosition _mousePosition;
    float _mouseWheel = 0.0F;
};

class FakeWindowPlugin final : public sq::engine::IPlugin {
  public:
    explicit FakeWindowPlugin(sq::engine::Key heldKey) : _heldKey(heldKey) {}
    [[nodiscard]] std::string_view name() const noexcept override { return "FakeWindowPlugin"; }
    void build(sq::engine::PluginBuilder& builder) override { builder.provides<sq::engine::IWindowService>(); }
    void init(sq::engine::Engine& engine) override {
        ASSERT_TRUE(
            engine.mediator().registerService<sq::engine::IWindowService>(std::make_shared<FakeWindow>(_heldKey)));
    }

  private:
    sq::engine::Key _heldKey;
};

}  // namespace

TEST(ActionId, Fnv1aHashConsistency) {
    constexpr sq::input::ActionId shootId = sq::input::makeActionId("shoot");
    EXPECT_EQ(shootId, sq::input::makeActionId("shoot"));
    EXPECT_NE(shootId, sq::input::makeActionId("move_x"));
    EXPECT_EQ(sq::input::FNV1A_OFFSET_BASIS_32, 2166136261U);
    EXPECT_EQ(sq::input::FNV1A_PRIME_32, 16777619U);
}

TEST(ActionMapping, DefaultConstructorIsEmpty) {
    const sq::input::ActionMapping mapping;

    EXPECT_FALSE(mapping.hasKey("move_up", sq::engine::Key::W));
    EXPECT_FALSE(mapping.hasKey("move_up", sq::engine::Key::Z));
    EXPECT_FALSE(mapping.hasKey("shoot", sq::engine::Key::Space));
    EXPECT_FLOAT_EQ(mapping.deadzone(), 0.2F);
}

TEST(ActionMapping, RuntimeBindingAndRemapping) {
    sq::input::ActionMapping mapping;

    mapping.registerAction("shoot", sq::input::ActionType::Button);
    EXPECT_TRUE(mapping.bindKey("shoot", sq::engine::Key::E));
    EXPECT_TRUE(mapping.hasKey("shoot", sq::engine::Key::E));

    EXPECT_TRUE(mapping.unbindKey("shoot", sq::engine::Key::E));
    EXPECT_FALSE(mapping.hasKey("shoot", sq::engine::Key::E));

    mapping.registerAction("move_up", sq::input::ActionType::Button);
    EXPECT_TRUE(mapping.bindKey("move_up", sq::engine::Key::W));
    EXPECT_TRUE(mapping.bindKey("move_up", sq::engine::Key::Z));
    EXPECT_TRUE(mapping.hasKey("move_up", sq::engine::Key::W));
    EXPECT_TRUE(mapping.hasKey("move_up", sq::engine::Key::Z));

    mapping.clearKeyBindings("move_up");
    EXPECT_FALSE(mapping.hasKey("move_up", sq::engine::Key::W));
    EXPECT_FALSE(mapping.hasKey("move_up", sq::engine::Key::Z));

    mapping.setDeadzone(0.35F);
    EXPECT_FLOAT_EQ(mapping.deadzone(), 0.35F);

    mapping.clear();
    EXPECT_FLOAT_EQ(mapping.deadzone(), 0.2F);
}

TEST(ActionMapping, CompositeAxisConfiguration) {
    sq::input::ActionMapping mapping;
    mapping.registerAction("move_x", sq::input::ActionType::Axis);
    mapping.bindAxisComposite("move_x", sq::engine::Key::A, sq::engine::Key::D);

    const auto* binding = mapping.getBinding(sq::input::makeActionId("move_x"));
    ASSERT_NE(binding, nullptr);
    EXPECT_EQ(binding->type, sq::input::ActionType::Axis);
    EXPECT_EQ(binding->negativeKeys.at(0), sq::engine::Key::A);
    EXPECT_EQ(binding->keys.at(0), sq::engine::Key::D);
}

TEST(InputState, DynamicActionsAndAxes) {
    sq::input::InputState state;

    EXPECT_FALSE(state.isDown("shoot"));
    EXPECT_FALSE(state.isJustPressed("shoot"));
    EXPECT_FALSE(state.isJustReleased("shoot"));
    EXPECT_FLOAT_EQ(state.getAxis("move_x"), 0.0F);
    EXPECT_FLOAT_EQ(state.getAxis("move_y"), 0.0F);

    state.setDown("shoot", true);
    state.setJustPressed("shoot", true);
    state.setAxis("move_x", 0.707F);
    state.setAxis("move_y", -0.707F);

    EXPECT_TRUE(state.isDown("shoot"));
    EXPECT_TRUE(state.isJustPressed("shoot"));
    EXPECT_FLOAT_EQ(state.getAxis("move_x"), 0.707F);
    EXPECT_FLOAT_EQ(state.getAxis("move_y"), -0.707F);

    state.resetFrameTransitions();
    EXPECT_TRUE(state.isDown("shoot"));
    EXPECT_FALSE(state.isJustPressed("shoot"));

    state.setDown("shoot", false);
    state.setJustReleased("shoot", true);
    EXPECT_FALSE(state.isDown("shoot"));
    EXPECT_TRUE(state.isJustReleased("shoot"));

    state.clear();
    EXPECT_FALSE(state.isDown("shoot"));
    EXPECT_FLOAT_EQ(state.getAxis("move_x"), 0.0F);
}

TEST(InputState, Vector2Normalization) {
    sq::input::InputState state;
    state.setAxis("move_x", 1.0F);
    state.setAxis("move_y", 1.0F);

    // Without normalization, magnitude is sqrt(2) ~ 1.414
    const auto unnormalized = state.getVector2("move_x", "move_y", false);
    EXPECT_FLOAT_EQ(unnormalized.x, 1.0F);
    EXPECT_FLOAT_EQ(unnormalized.y, 1.0F);

    // With normalization, magnitude is clamped to 1.0F
    const auto normalized = state.getVector2("move_x", "move_y", true);
    const float length = std::sqrt((normalized.x * normalized.x) + (normalized.y * normalized.y));
    EXPECT_NEAR(length, 1.0F, 0.0001F);
    EXPECT_NEAR(normalized.x, 0.7071F, 0.001F);
    EXPECT_NEAR(normalized.y, 0.7071F, 0.001F);
}

TEST(InputActionEvent, DesignatedInitializers) {
    const sq::events::InputActionEvent event{
        .action = sq::input::makeActionId("shoot"),
        .state = sq::input::ActionState::Pressed,
        .value = 1.0F,
        .playerIndex = 1,
    };
    EXPECT_EQ(event.action, sq::input::makeActionId("shoot"));
    EXPECT_EQ(event.state, sq::input::ActionState::Pressed);
    EXPECT_FLOAT_EQ(event.value, 1.0F);
    EXPECT_EQ(event.playerIndex, 1);
}

TEST(InputSystem, UpdatesComponentForMatchingPlayerIndex) {
    sq::ecs::Registry registry;
    sq::events::EventBus eventBus;
    sq::systems::InputSystem system0(sq::input::ActionMapping{}, 0, 0);
    sq::systems::InputSystem system1(sq::input::ActionMapping{}, 1, 1);

    const sq::ecs::Entity player0 = registry.spawnEntity();
    registry.addComponent<sq::input::InputState>(player0, sq::input::InputState{.playerIndex = 0, .actions = {}});

    const sq::ecs::Entity player1 = registry.spawnEntity();
    registry.addComponent<sq::input::InputState>(player1, sq::input::InputState{.playerIndex = 1, .actions = {}});

    const FakeWindow window;
    system0.update(window, registry, &eventBus);
    system1.update(window, registry, &eventBus);

    const auto& p0State = registry.getComponents<sq::input::InputState>().get(player0);
    EXPECT_EQ(p0State.playerIndex, 0);

    const auto& p1State = registry.getComponents<sq::input::InputState>().get(player1);
    EXPECT_EQ(p1State.playerIndex, 1);
}

TEST(InputSystem, DoesNothingWhenNoEntityHasInputState) {
    sq::ecs::Registry registry;
    sq::events::EventBus eventBus;
    sq::systems::InputSystem system(sq::input::ActionMapping{}, 0, 0);

    bool eventTriggered = false;
    eventBus.subscribe<sq::events::InputActionEvent>(
        [&eventTriggered](const sq::events::InputActionEvent& /*event*/) { eventTriggered = true; });

    const FakeWindow window;
    system.update(window, registry, &eventBus);
    EXPECT_FALSE(registry.hasResource<sq::input::InputState>());
    EXPECT_FALSE(eventTriggered);
}

TEST(InputSystem, ConfigurablePlayerAndGamepadIndices) {
    const sq::systems::InputSystem defaultSystem;
    EXPECT_EQ(defaultSystem.playerIndex(), 0);
    EXPECT_EQ(defaultSystem.gamepadIndex(), 0);

    sq::systems::InputSystem customSystem(sq::input::ActionMapping{}, 2, 1);
    EXPECT_EQ(customSystem.playerIndex(), 2);
    EXPECT_EQ(customSystem.gamepadIndex(), 1);

    customSystem.setPlayerIndex(3);
    customSystem.setGamepadIndex(2);
    EXPECT_EQ(customSystem.playerIndex(), 3);
    EXPECT_EQ(customSystem.gamepadIndex(), 2);
}

TEST(KeyboardLayout, DetectLayoutAndString) {
    EXPECT_STREQ(sq::input::keyboardLayoutToString(sq::input::KeyboardLayout::Qwerty), "QWERTY");
    EXPECT_STREQ(sq::input::keyboardLayoutToString(sq::input::KeyboardLayout::Azerty), "AZERTY");
    EXPECT_STREQ(sq::input::keyboardLayoutToString(static_cast<sq::input::KeyboardLayout>(99)), "UNKNOWN");

    const sq::input::KeyboardLayout detected = sq::input::detectKeyboardLayout();
    EXPECT_TRUE(detected == sq::input::KeyboardLayout::Qwerty || detected == sq::input::KeyboardLayout::Azerty);
}

TEST(KeyboardLayout, DetailIsAzertyLocale) {
    EXPECT_TRUE(sq::input::detail::isAzertyLocale("fr_FR.UTF-8"));
    EXPECT_TRUE(sq::input::detail::isAzertyLocale("fr_BE.UTF-8"));
    EXPECT_TRUE(sq::input::detail::isAzertyLocale("fr-FR"));
    EXPECT_TRUE(sq::input::detail::isAzertyLocale("fr-BE"));
    EXPECT_FALSE(sq::input::detail::isAzertyLocale("fr_CA.UTF-8"));
    EXPECT_FALSE(sq::input::detail::isAzertyLocale("fr-CA"));
    EXPECT_FALSE(sq::input::detail::isAzertyLocale("fr_CH.UTF-8"));
    EXPECT_FALSE(sq::input::detail::isAzertyLocale("fr_LU.UTF-8"));
    EXPECT_FALSE(sq::input::detail::isAzertyLocale("en_US.UTF-8"));
    EXPECT_FALSE(sq::input::detail::isAzertyLocale("C"));
    EXPECT_FALSE(sq::input::detail::isAzertyLocale(""));

    EXPECT_EQ(sq::input::WIN_LANGID_FRENCH_FRANCE, 0x040CU);
    EXPECT_EQ(sq::input::WIN_LANGID_FRENCH_BELGIUM, 0x080CU);
}

TEST(KeyboardLayout, DetailDetectFromLinuxConfig) {
#ifdef __linux__
    const auto layout = sq::input::detail::detectFromLinuxConfig();
    if (layout.has_value()) {
        EXPECT_TRUE(*layout == sq::input::KeyboardLayout::Azerty || *layout == sq::input::KeyboardLayout::Qwerty);
    }
#endif
}

TEST(ActionMapping, SamplesKeysThroughWindowService) {
    sq::input::ActionMapping mapping;
    mapping.registerAction("shoot", sq::input::ActionType::Button);
    mapping.bindKey("shoot", sq::engine::Key::Space);
    mapping.registerAction("move_x", sq::input::ActionType::Axis);
    mapping.bindAxisComposite("move_x", sq::engine::Key::A, sq::engine::Key::D);

    const FakeWindow holdingSpace(sq::engine::Key::Space);
    EXPECT_TRUE(mapping.isActionDown(holdingSpace, "shoot"));

    const FakeWindow holdingA(sq::engine::Key::A);
    EXPECT_FALSE(mapping.isActionDown(holdingA, "shoot"));
    EXPECT_FLOAT_EQ(mapping.getAxis(holdingA, "move_x"), -1.0F);
}

TEST(InputPlugin, RequiresAWindowServiceProvider) {
    sq::engine::Engine engine;
    engine.addPlugin<sq::input::InputPlugin>();

    EXPECT_THROW(engine.init(), sq::engine::PluginDependencyException);
}

TEST(InputPlugin, PollsAnyWindowServiceProvider) {
    sq::input::ActionMapping mapping;
    mapping.registerAction("shoot", sq::input::ActionType::Button);
    mapping.bindKey("shoot", sq::engine::Key::Space);

    sq::engine::Engine engine;
    engine.addPlugin<sq::input::InputPlugin>(mapping);
    engine.addPlugin<FakeWindowPlugin>(sq::engine::Key::Space);

    const sq::ecs::Entity player = engine.registry().spawnEntity();
    engine.registry().addComponent<sq::input::InputState>(player,
                                                          sq::input::InputState{.playerIndex = 0, .actions = {}});

    engine.advance();

    EXPECT_TRUE(engine.registry().getComponents<sq::input::InputState>().get(player).isDown("shoot"));
}

TEST(ActionMapping, RejectsUnknownInputCodes) {
    sq::input::ActionMapping mapping;
    mapping.registerAction("shoot", sq::input::ActionType::Button);

    EXPECT_FALSE(mapping.bindKey("shoot", sq::engine::Key::Unknown));
    EXPECT_FALSE(mapping.bindGamepadButton("shoot", sq::engine::GamepadButton::Unknown));
}

TEST(ActionMapping, SamplesGamepadThroughWindowService) {
    sq::input::ActionMapping mapping;
    mapping.registerAction("shoot", sq::input::ActionType::Button);
    mapping.bindGamepadButton("shoot", sq::engine::GamepadButton::RightFaceDown);
    mapping.registerAction("move_x", sq::input::ActionType::Axis);
    mapping.bindGamepadAxis("move_x", sq::engine::GamepadAxis::LeftX, 1.0F);

    FakeWindow window;
    EXPECT_FALSE(mapping.isActionDown(window, "shoot"));
    EXPECT_FLOAT_EQ(mapping.getAxis(window, "move_x"), 0.0F);

    window.holdButton(sq::engine::GamepadButton::RightFaceDown);
    window.tiltAxis(sq::engine::GamepadAxis::LeftX, 0.5F);
    EXPECT_TRUE(mapping.isActionDown(window, "shoot"));
    EXPECT_FLOAT_EQ(mapping.getAxis(window, "move_x"), 0.5F);
}

TEST(ActionMapping, SamplesMouseButtonsThroughWindowService) {
    sq::input::ActionMapping mapping;
    mapping.registerAction("shoot", sq::input::ActionType::Button);
    EXPECT_FALSE(mapping.bindMouseButton("shoot", sq::engine::MouseButton::Unknown));
    EXPECT_TRUE(mapping.bindMouseButton("shoot", sq::engine::MouseButton::Left));
    EXPECT_FALSE(mapping.bindMouseButton("shoot", sq::engine::MouseButton::Left));
    EXPECT_TRUE(mapping.bindMouseButton("shoot", sq::engine::MouseButton::Right));
    EXPECT_FALSE(mapping.bindMouseButton("shoot", sq::engine::MouseButton::Middle));

    FakeWindow window;
    EXPECT_FALSE(mapping.isActionDown(window, "shoot"));

    window.holdMouseButton(sq::engine::MouseButton::Right);
    EXPECT_TRUE(mapping.isActionDown(window, "shoot"));
}

TEST(InputSystem, UpdatesMouseStateOfMatchingPlayerOnly) {
    sq::ecs::Registry registry;
    sq::systems::InputSystem system(sq::input::ActionMapping{}, 0, 0);

    const sq::ecs::Entity player0 = registry.spawnEntity();
    registry.addComponent<sq::input::InputState>(player0, sq::input::InputState{.playerIndex = 0, .actions = {}});
    const sq::ecs::Entity player1 = registry.spawnEntity();
    registry.addComponent<sq::input::InputState>(player1, sq::input::InputState{.playerIndex = 1, .actions = {}});

    FakeWindow window;
    window.moveMouse(sq::engine::MousePosition{.x = 12.0F, .y = 34.0F}, 2.0F);
    system.update(window, registry);

    const auto& mouse = registry.getComponents<sq::input::InputState>().get(player0).mouse;
    EXPECT_FLOAT_EQ(mouse.x, 12.0F);
    EXPECT_FLOAT_EQ(mouse.y, 34.0F);
    EXPECT_FLOAT_EQ(mouse.wheel, 2.0F);
    EXPECT_FLOAT_EQ(registry.getComponents<sq::input::InputState>().get(player1).mouse.x, 0.0F);
}

TEST(KeyNames, EveryKeyIdReadsBackAsItsKey) {
    for (const sq::input::KeyName& name : sq::input::keyNames()) {
        EXPECT_EQ(sq::input::keyFromId(sq::input::keyId(name.key)), name.key) << name.id;
    }
    EXPECT_FALSE(sq::input::keyFromId("NotAKey").has_value());
    EXPECT_TRUE(sq::input::keyId(sq::engine::Key::Unknown).empty());
}

TEST(KeyNames, LabelsFollowTheKeyboardLayout) {
    EXPECT_EQ(sq::input::keyLabel(sq::engine::Key::W, sq::input::KeyboardLayout::Qwerty), "W");
    EXPECT_EQ(sq::input::keyLabel(sq::engine::Key::W, sq::input::KeyboardLayout::Azerty), "Z");
    EXPECT_EQ(sq::input::keyLabel(sq::engine::Key::A, sq::input::KeyboardLayout::Azerty), "Q");
    EXPECT_EQ(sq::input::keyLabel(sq::engine::Key::LeftShift, sq::input::KeyboardLayout::Qwerty), "LEFT SHIFT");
    EXPECT_EQ(sq::input::keyLabel(sq::engine::Key::Unknown, sq::input::KeyboardLayout::Qwerty), "NONE");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
