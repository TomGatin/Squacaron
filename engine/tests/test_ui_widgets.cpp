#include <gtest/gtest.h>

#include <deque>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "hierarchy/Hierarchy.hpp"
#include "hierarchy/HierarchyHooks.hpp"
#include "render/api/IScreenDrawService.hpp"
#include "render/api/IWindowService.hpp"
#include "render/api/InputCodes.hpp"
#include "render/api/components/Hidden.hpp"
#include "render/api/components/Renderable.hpp"
#include "ui/UiEvents.hpp"
#include "ui/components/UiAnchor.hpp"
#include "ui/components/UiButton.hpp"
#include "ui/components/UiFocusable.hpp"
#include "ui/components/UiImage.hpp"
#include "ui/components/UiNavigation.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiScrollList.hpp"
#include "ui/components/UiSlider.hpp"
#include "ui/components/UiText.hpp"
#include "ui/components/UiTextInput.hpp"
#include "ui/components/UiToggle.hpp"
#include "ui/system/UiInteractionSystem.hpp"
#include "ui/system/UiLayout.hpp"
#include "ui/system/UiRenderSystem.hpp"

namespace {

/** @brief Window whose keys, mouse, wheel and typed characters are set by the test. */
struct FakeWindow final : sq::engine::IWindowService {
    std::set<sq::engine::Key> keys;
    std::set<sq::engine::GamepadButton> pad;
    float mx = -1.0F;
    float my = -1.0F;
    bool leftDown = false;
    bool leftPressed = false;
    float wheel = 0.0F;
    std::deque<char32_t> typed;

    [[nodiscard]] bool shouldClose() const noexcept override { return false; }
    void beginFrame() override {}
    void endFrame() override {}
    [[nodiscard]] int width() const noexcept override { return 1000; }
    [[nodiscard]] int height() const noexcept override { return 1000; }
    [[nodiscard]] bool isKeyDown(sq::engine::Key key) const noexcept override { return keys.contains(key); }
    [[nodiscard]] bool isGamepadAvailable(int /*gamepad*/) const noexcept override { return true; }
    [[nodiscard]] bool isGamepadButtonDown(int /*gamepad*/, sq::engine::GamepadButton button) const noexcept override {
        return pad.contains(button);
    }
    [[nodiscard]] float gamepadAxis(int /*gamepad*/, sq::engine::GamepadAxis /*axis*/) const noexcept override {
        return 0.0F;
    }
    [[nodiscard]] bool isMouseButtonDown(sq::engine::MouseButton button) const noexcept override {
        return button == sq::engine::MouseButton::Left && leftDown;
    }
    [[nodiscard]] bool isMouseButtonPressed(sq::engine::MouseButton button) const noexcept override {
        return button == sq::engine::MouseButton::Left && leftPressed;
    }
    [[nodiscard]] bool isMouseButtonReleased(sq::engine::MouseButton /*button*/) const noexcept override {
        return false;
    }
    [[nodiscard]] sq::engine::MousePosition mousePosition() const noexcept override { return {.x = mx, .y = my}; }
    [[nodiscard]] float mouseWheelDelta() const noexcept override { return wheel; }
    [[nodiscard]] char32_t nextTypedCharacter() noexcept override {
        if (typed.empty()) {
            return U'\0';
        }
        const char32_t cp = typed.front();
        typed.pop_front();
        return cp;
    }
};

/** @brief Draw service recording fills and strokes. */
struct RecordingDraw final : sq::engine::IScreenDrawService {
    std::vector<std::string> calls;
    sq::engine::FontId loadFont(const std::string& /*path*/) override { return sq::engine::NO_FONT; }
    [[nodiscard]] sq::engine::TextSize measureText(std::string_view text, float /*size*/, float /*spacing*/,
                                                   sq::engine::FontId /*font*/) const override {
        return {.width = 10.0F * static_cast<float>(text.size()), .height = 10.0F};
    }
    void fillRect(const sq::engine::ScreenRect& /*rect*/, const sq::ecs::Rgba& /*color*/) override {
        calls.emplace_back("fill");
    }
    void strokeRect(const sq::engine::ScreenRect& /*rect*/, float /*thickness*/,
                    const sq::ecs::Rgba& /*color*/) override {
        calls.emplace_back("stroke");
    }
    void drawText(std::string_view text, float /*x*/, float /*y*/, float /*size*/, float /*spacing*/,
                  const sq::ecs::Rgba& /*color*/, sq::engine::FontId /*font*/) override {
        calls.push_back("text:" + std::string(text));
    }
    void drawTexture(sq::ecs::TextureId /*texture*/, const sq::ecs::TextureRect& /*source*/,
                     const sq::engine::ScreenRect& /*dest*/, float /*rotation*/,
                     const sq::ecs::Rgba& /*tint*/) override {
        calls.emplace_back("texture");
    }
};

sq::ui::UiSize fixedMeasure(const sq::ui::UiText& /*text*/) { return {.width = 10.0F, .height = 10.0F}; }

/** @brief Registry with hierarchy hooks, a fake window, a bus recording every UI event, and the interaction system. */
class UiWidgetsTest : public ::testing::Test {
  protected:
    void SetUp() override {
        registry.emplaceResource<sq::ui::UiFocus>();
        registry.emplaceResource<sq::ui::UiNavigation>();
        (void)bus.subscribe<sq::ui::UiButtonClickedEvent>(
            [this](const sq::ui::UiButtonClickedEvent& e) { events.push_back("click:" + e.action); });
        (void)bus.subscribe<sq::ui::UiToggleChangedEvent>([this](const sq::ui::UiToggleChangedEvent& e) {
            events.push_back("toggle:" + e.action + (e.on ? ":on" : ":off"));
        });
        (void)bus.subscribe<sq::ui::UiValueChangedEvent>([this](const sq::ui::UiValueChangedEvent& e) {
            events.push_back("value:" + e.action + ":" + std::to_string(static_cast<int>(e.value * 100.0F)));
        });
        (void)bus.subscribe<sq::ui::UiTextSubmittedEvent>(
            [this](const sq::ui::UiTextSubmittedEvent& e) { events.push_back("submit:" + e.action + ":" + e.text); });
        (void)bus.subscribe<sq::ui::UiBackEvent>(
            [this](const sq::ui::UiBackEvent& /*e*/) { events.emplace_back("back"); });
    }

    sq::ecs::Entity widget(float y, int order = 0) {
        const sq::ecs::Entity e = registry.spawnEntity();
        registry.addComponent(e, sq::ui::UiAnchor{.offsetX = 10.0F, .offsetY = y});
        registry.addComponent(e, sq::ui::UiFocusable{.order = order});
        return e;
    }

    sq::ecs::Entity button(float y, const std::string& action, int order = 0) {
        const sq::ecs::Entity e = widget(y, order);
        registry.addComponent(e, sq::ui::UiButton{.width = 100.0F, .height = 30.0F, .action = action});
        return e;
    }

    std::vector<sq::ui::UiLayoutNode> layout() {
        std::vector<sq::ui::UiLayoutNode> nodes;
        sq::ui::computeLayout(registry, sq::ui::UiScreenRect{.width = 1000.0F, .height = 1000.0F}, fixedMeasure, nodes);
        return nodes;
    }

    /** @brief Runs one frame of interactions. */
    void frame(float dt = 0.016F) {
        const auto nodes = layout();
        system.update(registry, nodes, window, bus, dt);
        window.leftPressed = false;
        window.wheel = 0.0F;
    }

    /** @brief Presses then releases a key over two frames. */
    void tap(sq::engine::Key key) {
        window.keys.insert(key);
        frame();
        window.keys.erase(key);
        frame();
    }

    void clickAt(float x, float y) {
        window.mx = x;
        window.my = y;
        window.leftDown = true;
        window.leftPressed = true;
        frame();
        window.leftDown = false;
        frame();
    }

    sq::ecs::Entity focused() { return registry.getResource<sq::ui::UiFocus>().focused; }

    sq::ecs::Registry registry;
    sq::hierarchy::HierarchyHooks hooks{registry};
    sq::events::EventBus bus;
    FakeWindow window;
    sq::ui::UiInteractionSystem system;
    std::vector<std::string> events;
};

TEST_F(UiWidgetsTest, HiddenElementIsSkippedWithItsSubtree) {
    const sq::ecs::Entity panel = widget(0.0F);
    const sq::ecs::Entity child = widget(10.0F);
    sq::hierarchy::setParent(registry, child, panel);
    EXPECT_EQ(layout().size(), 2U);

    sq::ecs::hide(registry, panel);
    EXPECT_TRUE(layout().empty());
    sq::ecs::show(registry, panel);
    EXPECT_EQ(layout().size(), 2U);
}

TEST_F(UiWidgetsTest, HiddenReasonsAreIndependent) {
    const sq::ecs::Entity e = widget(0.0F);
    sq::ecs::hide(registry, e, sq::ecs::HiddenReason::Manual);
    sq::ecs::hide(registry, e, sq::ecs::HiddenReason::Scene);
    sq::ecs::show(registry, e, sq::ecs::HiddenReason::Scene);
    EXPECT_TRUE(sq::ecs::isHidden(registry, e));
    sq::ecs::show(registry, e, sq::ecs::HiddenReason::Manual);
    EXPECT_FALSE(sq::ecs::isHidden(registry, e));
}

TEST_F(UiWidgetsTest, ScrollListLaysOutOnlyTheRowsInViewInTheirSlots) {
    const sq::ecs::Entity list = registry.spawnEntity();
    registry.addComponent(list, sq::ui::UiAnchor{.offsetX = 0.0F, .offsetY = 100.0F});
    registry.addComponent(list, sq::ui::UiRect{.width = 200.0F, .height = 90.0F});
    registry.addComponent(list, sq::ui::UiScrollList{.first = 1, .visibleCount = 2, .itemHeight = 30.0F});
    std::vector<sq::ecs::Entity> rows;
    for (int i = 0; i < 4; ++i) {
        const sq::ecs::Entity row = button(0.0F, "row" + std::to_string(i));
        registry.getComponents<sq::ui::UiAnchor>().get(row).offsetX = 0.0F;
        sq::hierarchy::setParent(registry, row, list);
        rows.push_back(row);
    }

    const auto nodes = layout();
    ASSERT_EQ(nodes.size(), 3U);
    EXPECT_EQ(nodes[1].entity, rows[1]);
    EXPECT_FLOAT_EQ(nodes[1].rect.y, 100.0F);
    EXPECT_EQ(nodes[2].entity, rows[2]);
    EXPECT_FLOAT_EQ(nodes[2].rect.y, 130.0F);

    registry.getComponents<sq::ui::UiScrollList>().get(list).first = 99;
    (void)layout();
    EXPECT_EQ(registry.getComponents<sq::ui::UiScrollList>().get(list).first, 2U);
}

TEST_F(UiWidgetsTest, DownAndUpWalkTheFocusableWidgetsByOrderAndWrap) {
    const sq::ecs::Entity second = button(100.0F, "b", 2);
    const sq::ecs::Entity first = button(0.0F, "a", 1);
    tap(sq::engine::Key::Down);
    EXPECT_EQ(focused(), first);
    tap(sq::engine::Key::Down);
    EXPECT_EQ(focused(), second);
    tap(sq::engine::Key::Down);
    EXPECT_EQ(focused(), first);
    tap(sq::engine::Key::Up);
    EXPECT_EQ(focused(), second);
}

TEST_F(UiWidgetsTest, HoldingAKeyMovesTheFocusOnce) {
    (void)button(0.0F, "a");
    const sq::ecs::Entity second = button(100.0F, "b");
    window.keys.insert(sq::engine::Key::Down);
    frame();
    frame();
    frame();
    EXPECT_NE(focused(), second);
}

TEST_F(UiWidgetsTest, ConfirmClicksTheFocusedButtonFromKeyboardOrGamepad) {
    (void)button(0.0F, "play");
    tap(sq::engine::Key::Down);
    tap(sq::engine::Key::Enter);
    window.pad.insert(sq::engine::GamepadButton::RightFaceDown);
    frame();
    EXPECT_EQ(events, (std::vector<std::string>{"click:play", "click:play"}));
}

TEST_F(UiWidgetsTest, BackPublishesABackEvent) {
    tap(sq::engine::Key::Escape);
    EXPECT_EQ(events, (std::vector<std::string>{"back"}));
}

TEST_F(UiWidgetsTest, ToggleFlipsOnClickAndOnConfirm) {
    const sq::ecs::Entity toggle = widget(0.0F);
    registry.addComponent(toggle, sq::ui::UiToggle{.action = "metrics"});
    clickAt(15.0F, 5.0F);
    EXPECT_EQ(focused(), toggle);
    tap(sq::engine::Key::Space);
    EXPECT_EQ(events, (std::vector<std::string>{"toggle:metrics:on", "toggle:metrics:off"}));
}

TEST_F(UiWidgetsTest, SliderMovesByStepAndStaysInRange) {
    const sq::ecs::Entity slider = widget(0.0F);
    registry.addComponent(slider, sq::ui::UiSlider{.value = 0.9F, .step = 0.25F, .action = "volume"});
    tap(sq::engine::Key::Down);
    ASSERT_EQ(focused(), slider);
    tap(sq::engine::Key::Right);
    tap(sq::engine::Key::Right);
    tap(sq::engine::Key::Left);
    EXPECT_EQ(events, (std::vector<std::string>{"value:volume:100", "value:volume:75"}));
}

TEST_F(UiWidgetsTest, SliderFollowsTheMouseWhileDragged) {
    const sq::ecs::Entity slider = widget(0.0F);
    registry.addComponent(slider, sq::ui::UiSlider{.action = "volume", .width = 100.0F});
    window.mx = 10.0F;
    window.my = 5.0F;
    window.leftDown = true;
    window.leftPressed = true;
    frame();
    window.mx = 60.0F;
    frame();
    EXPECT_FLOAT_EQ(registry.getComponents<sq::ui::UiSlider>().get(slider).value, 0.5F);
    window.leftDown = false;
    window.mx = 110.0F;
    frame();
    EXPECT_FLOAT_EQ(registry.getComponents<sq::ui::UiSlider>().get(slider).value, 0.5F);
}

TEST_F(UiWidgetsTest, TextInputTypesFiltersAndSubmits) {
    const sq::ecs::Entity input = widget(0.0F);
    registry.addComponent(input, sq::ui::UiTextInput{.action = "host", .allowed = "0123456789.", .maxLength = 9});
    clickAt(15.0F, 5.0F);
    ASSERT_EQ(focused(), input);

    window.typed = {U'1', U'2', U'x', U'7', U'.', U'0', U' ', U'.', U'0', U'.', U'1', U'9'};
    frame();
    EXPECT_EQ(registry.getComponents<sq::ui::UiTextInput>().get(input).text, "127.0.0.1");

    tap(sq::engine::Key::Space);  // types (filtered here), never submits
    tap(sq::engine::Key::Enter);
    EXPECT_EQ(events, (std::vector<std::string>{"submit:host:127.0.0.1"}));
}

TEST_F(UiWidgetsTest, TextInputCanTurnLettersIntoCapitals) {
    const sq::ecs::Entity input = widget(0.0F);
    registry.addComponent(input, sq::ui::UiTextInput{.allowed = "ABC", .uppercase = true});
    tap(sq::engine::Key::Down);
    window.typed = {U'a', U'B', U'c', U'd'};
    frame();
    EXPECT_EQ(registry.getComponents<sq::ui::UiTextInput>().get(input).text, "ABC");
}

TEST_F(UiWidgetsTest, ImageIsLaidOutAndDrawnWithItsSize) {
    const sq::ecs::Entity image = registry.spawnEntity();
    registry.addComponent(image, sq::ui::UiAnchor{});
    registry.addComponent(image, sq::ui::UiImage{.texture = 3, .width = 40.0F, .height = 20.0F});
    const auto nodes = layout();
    ASSERT_EQ(nodes.size(), 1U);
    EXPECT_FLOAT_EQ(nodes[0].rect.width, 40.0F);
    RecordingDraw draw;
    sq::ui::UiRenderSystem renderer;
    renderer.update(registry, sq::ui::UiScreenRect{.width = 1000.0F, .height = 1000.0F}, draw);
    EXPECT_EQ(draw.calls, (std::vector<std::string>{"texture"}));
}

TEST_F(UiWidgetsTest, TextInputAcceptsUtf8AndErasesWholeCharacters) {
    const sq::ecs::Entity input = widget(0.0F);
    registry.addComponent(input, sq::ui::UiTextInput{.action = "name"});
    tap(sq::engine::Key::Down);
    // Code points rather than U'\u...' literals, which MSVC rejects without /utf-8.
    window.typed = {U'J', static_cast<char32_t>(0xE9), static_cast<char32_t>(0x20AC)};
    frame();
    EXPECT_EQ(registry.getComponents<sq::ui::UiTextInput>().get(input).text, "J\xc3\xa9\xe2\x82\xac");
    tap(sq::engine::Key::Backspace);
    EXPECT_EQ(registry.getComponents<sq::ui::UiTextInput>().get(input).text, "J\xc3\xa9");
}

TEST_F(UiWidgetsTest, BackspaceRepeatsWhileHeld) {
    const sq::ecs::Entity input = widget(0.0F);
    registry.addComponent(input, sq::ui::UiTextInput{.text = "abcdef"});
    tap(sq::engine::Key::Down);
    window.keys.insert(sq::engine::Key::Backspace);
    frame(0.01F);  // first erase on the press
    frame(0.30F);  // still in the delay
    frame(0.20F);  // delay over: repeat
    frame(0.05F);  // and again
    EXPECT_EQ(registry.getComponents<sq::ui::UiTextInput>().get(input).text, "abc");
}

TEST_F(UiWidgetsTest, ClickingNothingClearsTheFocus) {
    const sq::ecs::Entity b = button(0.0F, "a");
    clickAt(15.0F, 5.0F);
    EXPECT_EQ(focused(), b);
    clickAt(900.0F, 900.0F);
    EXPECT_EQ(focused(), sq::ecs::NULL_ENTITY);
}

TEST_F(UiWidgetsTest, HiddenFocusedWidgetLosesTheFocus) {
    const sq::ecs::Entity b = button(0.0F, "a");
    tap(sq::engine::Key::Down);
    ASSERT_EQ(focused(), b);
    sq::ecs::hide(registry, b);
    frame();
    EXPECT_EQ(focused(), sq::ecs::NULL_ENTITY);
}

TEST_F(UiWidgetsTest, NavigationInsideAListScrollsIt) {
    const sq::ecs::Entity list = registry.spawnEntity();
    registry.addComponent(list, sq::ui::UiAnchor{});
    registry.addComponent(list, sq::ui::UiRect{.width = 200.0F, .height = 60.0F});
    registry.addComponent(list, sq::ui::UiScrollList{.visibleCount = 2, .itemHeight = 30.0F});
    std::vector<sq::ecs::Entity> rows;
    for (int i = 0; i < 4; ++i) {
        const sq::ecs::Entity row = button(0.0F, "row");
        sq::hierarchy::setParent(registry, row, list);
        rows.push_back(row);
    }
    auto& scroll = registry.getComponents<sq::ui::UiScrollList>().get(list);

    tap(sq::engine::Key::Down);
    EXPECT_EQ(focused(), rows[0]);
    tap(sq::engine::Key::Down);
    tap(sq::engine::Key::Down);
    EXPECT_EQ(focused(), rows[2]);
    EXPECT_EQ(scroll.first, 1U);
    tap(sq::engine::Key::Down);
    EXPECT_EQ(focused(), rows[3]);
    EXPECT_EQ(scroll.first, 2U);
    tap(sq::engine::Key::Up);
    tap(sq::engine::Key::Up);
    tap(sq::engine::Key::Up);
    EXPECT_EQ(focused(), rows[0]);
    EXPECT_EQ(scroll.first, 0U);
}

TEST_F(UiWidgetsTest, NavigationInsideAListSkipsRowsThatCannotTakeTheFocus) {
    const sq::ecs::Entity list = registry.spawnEntity();
    registry.addComponent(list, sq::ui::UiAnchor{});
    registry.addComponent(list, sq::ui::UiRect{.width = 200.0F, .height = 60.0F});
    registry.addComponent(list, sq::ui::UiScrollList{.visibleCount = 2, .itemHeight = 30.0F});
    std::vector<sq::ecs::Entity> rows;
    for (int i = 0; i < 5; ++i) {
        // Only the first and the last row are focusable: the three between are disabled entries.
        const sq::ecs::Entity row = (i == 0 || i == 4) ? button(0.0F, "row") : registry.spawnEntity();
        if (i != 0 && i != 4) {
            registry.addComponent(row, sq::ui::UiAnchor{});
            registry.addComponent(row, sq::ui::UiRect{.width = 200.0F, .height = 30.0F});
        }
        sq::hierarchy::setParent(registry, row, list);
        rows.push_back(row);
    }
    auto& scroll = registry.getComponents<sq::ui::UiScrollList>().get(list);

    tap(sq::engine::Key::Down);
    EXPECT_EQ(focused(), rows[0]);
    tap(sq::engine::Key::Down);
    EXPECT_EQ(focused(), rows[4]);
    EXPECT_EQ(scroll.first, 3U);  // scrolled past the disabled rows to show the next focusable one
    tap(sq::engine::Key::Up);
    EXPECT_EQ(focused(), rows[0]);
    EXPECT_EQ(scroll.first, 0U);
}

TEST_F(UiWidgetsTest, WheelScrollsTheListUnderTheCursor) {
    const sq::ecs::Entity list = registry.spawnEntity();
    registry.addComponent(list, sq::ui::UiAnchor{});
    registry.addComponent(list, sq::ui::UiRect{.width = 200.0F, .height = 60.0F});
    registry.addComponent(list, sq::ui::UiScrollList{.visibleCount = 2, .itemHeight = 30.0F});
    for (int i = 0; i < 3; ++i) {
        sq::hierarchy::setParent(registry, button(0.0F, "row"), list);
    }
    auto& scroll = registry.getComponents<sq::ui::UiScrollList>().get(list);
    window.mx = 50.0F;
    window.my = 20.0F;
    window.wheel = -1.0F;
    frame();
    EXPECT_EQ(scroll.first, 1U);
    window.wheel = -1.0F;
    frame();
    EXPECT_EQ(scroll.first, 1U);
    window.wheel = 1.0F;
    frame();
    EXPECT_EQ(scroll.first, 0U);
}

TEST_F(UiWidgetsTest, RendererDrawsTheFocusOutlineAndSkipsHiddenWidgets) {
    const sq::ecs::Entity b = button(0.0F, "a");
    const sq::ecs::Entity hidden = registry.spawnEntity();
    registry.addComponent(hidden, sq::ui::UiAnchor{});
    registry.addComponent(hidden, sq::ui::UiText{.text = "secret"});
    sq::ecs::hide(registry, hidden);
    registry.getResource<sq::ui::UiFocus>().focused = b;

    RecordingDraw draw;
    sq::ui::UiRenderSystem renderer;
    renderer.update(registry, sq::ui::UiScreenRect{.width = 1000.0F, .height = 1000.0F}, draw);
    EXPECT_EQ(draw.calls, (std::vector<std::string>{"fill", "stroke"}));
}

TEST_F(UiWidgetsTest, RendererDrawsTextInputPlaceholderAndCaret) {
    const sq::ecs::Entity input = widget(0.0F);
    registry.addComponent(input, sq::ui::UiTextInput{.placeholder = "host"});
    auto& focus = registry.getResource<sq::ui::UiFocus>();
    focus.focused = input;
    focus.caretVisible = true;
    registry.getResource<sq::ui::UiNavigation>().focusThickness = 0.0F;

    RecordingDraw draw;
    sq::ui::UiRenderSystem renderer;
    renderer.update(registry, sq::ui::UiScreenRect{.width = 1000.0F, .height = 1000.0F}, draw);
    EXPECT_EQ(draw.calls, (std::vector<std::string>{"fill", "stroke", "text:host", "fill"}));
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
