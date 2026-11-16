#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "hierarchy/Hierarchy.hpp"
#include "hierarchy/HierarchyHooks.hpp"
#include "render/api/IScreenDrawService.hpp"
#include "render/api/IWindowService.hpp"
#include "render/api/components/Renderable.hpp"
#include "ui/UiEvents.hpp"
#include "ui/components/FontId.hpp"
#include "ui/components/UiAnchor.hpp"
#include "ui/components/UiButton.hpp"
#include "ui/components/UiProgressBar.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiText.hpp"
#include "ui/system/UiButtonSystem.hpp"
#include "ui/system/UiLayout.hpp"
#include "ui/system/UiRenderSystem.hpp"

namespace {

struct FakeWindow : sq::engine::IWindowService {
    int w = 1920;
    int h = 1080;
    float mx = 0.0F;
    float my = 0.0F;
    bool leftDown = false;
    bool leftPressed = false;
    bool leftReleased = false;

    /** @brief Simulates the frame the left button goes down. */
    void press() {
        leftDown = true;
        leftPressed = true;
        leftReleased = false;
    }

    /** @brief Simulates the frame the left button goes up. */
    void release() {
        leftDown = false;
        leftPressed = false;
        leftReleased = true;
    }

    [[nodiscard]] bool shouldClose() const noexcept override { return false; }
    void beginFrame() override {}
    void endFrame() override {}
    [[nodiscard]] int width() const noexcept override { return w; }
    [[nodiscard]] int height() const noexcept override { return h; }
    [[nodiscard]] bool isKeyDown(sq::engine::Key) const noexcept override { return false; }
    [[nodiscard]] bool isGamepadAvailable(int) const noexcept override { return false; }
    [[nodiscard]] bool isGamepadButtonDown(int, sq::engine::GamepadButton) const noexcept override { return false; }
    [[nodiscard]] float gamepadAxis(int, sq::engine::GamepadAxis) const noexcept override { return 0.0F; }
    [[nodiscard]] bool isMouseButtonDown(sq::engine::MouseButton btn) const noexcept override {
        return btn == sq::engine::MouseButton::Left && leftDown;
    }
    [[nodiscard]] bool isMouseButtonPressed(sq::engine::MouseButton btn) const noexcept override {
        return btn == sq::engine::MouseButton::Left && leftPressed;
    }
    [[nodiscard]] bool isMouseButtonReleased(sq::engine::MouseButton btn) const noexcept override {
        return btn == sq::engine::MouseButton::Left && leftReleased;
    }
    [[nodiscard]] sq::engine::MousePosition mousePosition() const noexcept override { return {.x = mx, .y = my}; }
    [[nodiscard]] float mouseWheelDelta() const noexcept override { return 0.0F; }
    [[nodiscard]] char32_t nextTypedCharacter() noexcept override { return U'\0'; }
};

static sq::ecs::Entity makeButton(sq::ecs::Registry& reg, float ancX, float ancY, float w = 120.0F, float h = 36.0F,
                                  const std::string& action = "click") {
    const sq::ecs::Entity e = reg.spawnEntity();
    reg.addComponent<sq::ui::UiAnchor>(e, sq::ui::UiAnchor{.anchorX = ancX, .anchorY = ancY});
    reg.addComponent<sq::ui::UiButton>(e, sq::ui::UiButton{.width = w, .height = h, .action = action});
    return e;
}

/** @brief Fixed text size, so layout tests do not need a window or a font. */
constexpr sq::ui::UiSize FAKE_TEXT_SIZE = {.width = 50.0F, .height = 10.0F};

sq::ui::UiSize fakeMeasure(const sq::ui::UiText& /*text*/) { return FAKE_TEXT_SIZE; }

std::vector<sq::ui::UiLayoutNode> layoutOf(sq::ecs::Registry& reg, const FakeWindow& win) {
    std::vector<sq::ui::UiLayoutNode> nodes;
    sq::ui::computeLayout(reg,
                          sq::ui::UiScreenRect{.width = static_cast<float>(win.w), .height = static_cast<float>(win.h)},
                          fakeMeasure, nodes);
    return nodes;
}

void updateButtons(sq::ecs::Registry& reg, const FakeWindow& win, sq::events::EventBus& bus) {
    sq::ui::UiButtonSystem::update(reg, layoutOf(reg, win), win, bus);
}

const sq::ui::UiLayoutNode* findNode(const std::vector<sq::ui::UiLayoutNode>& nodes, sq::ecs::Entity e) {
    const auto it = std::ranges::find(nodes, e, &sq::ui::UiLayoutNode::entity);
    return it == nodes.end() ? nullptr : &*it;
}

std::ptrdiff_t indexOf(const std::vector<sq::ui::UiLayoutNode>& nodes, sq::ecs::Entity e) {
    return std::ranges::find(nodes, e, &sq::ui::UiLayoutNode::entity) - nodes.begin();
}

/** @brief Records the draw calls it receives, in order. */
struct FakeScreenDraw final : sq::engine::IScreenDrawService {
    struct Call {
        std::string kind;
        sq::engine::ScreenRect rect;
        sq::ecs::Rgba color;
        std::string text;
    };
    std::vector<Call> calls;

    sq::engine::FontId loadFont(const std::string& /*path*/) override { return sq::engine::NO_FONT; }
    [[nodiscard]] sq::engine::TextSize measureText(std::string_view /*text*/, float /*fontSize*/, float /*spacing*/,
                                                   sq::engine::FontId /*font*/) const override {
        return {.width = FAKE_TEXT_SIZE.width, .height = FAKE_TEXT_SIZE.height};
    }
    void fillRect(const sq::engine::ScreenRect& rect, const sq::ecs::Rgba& color) override {
        calls.push_back({.kind = "fill", .rect = rect, .color = color, .text = {}});
    }
    void strokeRect(const sq::engine::ScreenRect& rect, float /*thickness*/, const sq::ecs::Rgba& color) override {
        calls.push_back({.kind = "stroke", .rect = rect, .color = color, .text = {}});
    }
    void drawText(std::string_view text, float x, float y, float /*fontSize*/, float /*spacing*/,
                  const sq::ecs::Rgba& color, sq::engine::FontId /*font*/) override {
        calls.push_back({.kind = "text", .rect = {.x = x, .y = y}, .color = color, .text = std::string(text)});
    }
    void drawTexture(sq::ecs::TextureId /*texture*/, const sq::ecs::TextureRect& /*source*/,
                     const sq::engine::ScreenRect& dest, float /*rotation*/, const sq::ecs::Rgba& tint) override {
        calls.push_back({.kind = "texture", .rect = dest, .color = tint, .text = {}});
    }
};

void renderUi(sq::ecs::Registry& reg, FakeScreenDraw& draw) {
    sq::ui::UiRenderSystem system;
    system.update(reg, sq::ui::UiScreenRect{.width = 1920.0F, .height = 1080.0F}, draw);
}

}  // namespace

TEST(FontId, NoFontSentinelIsZero) { EXPECT_EQ(sq::ui::NO_FONT, 0U); }

TEST(UiAnchor, DefaultValues) {
    const sq::ui::UiAnchor anchor;

    EXPECT_EQ(anchor.anchorX, 0.0F);
    EXPECT_EQ(anchor.anchorY, 0.0F);
    EXPECT_EQ(anchor.offsetX, 0.0F);
    EXPECT_EQ(anchor.offsetY, 0.0F);
}

TEST(UiAnchor, DesignatedInitializersKeepOtherDefaults) {
    const sq::ui::UiAnchor anchor{.anchorX = 0.5F, .anchorY = 1.0F};

    EXPECT_EQ(anchor.anchorX, 0.5F);
    EXPECT_EQ(anchor.anchorY, 1.0F);
    EXPECT_EQ(anchor.offsetX, 0.0F);
    EXPECT_EQ(anchor.offsetY, 0.0F);
}

TEST(UiAnchor, WorksAsEcsComponent) {
    sq::ecs::Registry registry;
    const sq::ecs::Entity entity = registry.spawnEntity();

    registry.addComponent<sq::ui::UiAnchor>(entity, sq::ui::UiAnchor{.anchorX = 0.5F, .offsetY = -8.0F});

    ASSERT_TRUE(registry.hasComponent<sq::ui::UiAnchor>(entity));
    EXPECT_EQ(registry.getComponents<sq::ui::UiAnchor>().get(entity).anchorX, 0.5F);
    EXPECT_EQ(registry.getComponents<sq::ui::UiAnchor>().get(entity).offsetY, -8.0F);
}

static std::pair<float, float> resolveAnchor(const sq::ui::UiAnchor& anchor, float screenW, float screenH,
                                             float elemW = 0.0F, float elemH = 0.0F) {
    const sq::ui::UiScreenRect rect =
        sq::ui::resolveAnchor(anchor, sq::ui::UiScreenRect{.width = screenW, .height = screenH},
                              sq::ui::UiSize{.width = elemW, .height = elemH});
    return {rect.x, rect.y};
}

TEST(UiAnchorResolution, TopLeftCorner) {
    const sq::ui::UiAnchor anchor{.anchorX = 0.0F, .anchorY = 0.0F};
    auto [x, y] = resolveAnchor(anchor, 1920.0F, 1080.0F);

    EXPECT_FLOAT_EQ(x, 0.0F);
    EXPECT_FLOAT_EQ(y, 0.0F);
}

TEST(UiAnchorResolution, BottomRightCornerNoPivot) {
    const sq::ui::UiAnchor anchor{.anchorX = 1.0F, .anchorY = 1.0F};
    auto [x, y] = resolveAnchor(anchor, 1920.0F, 1080.0F);

    EXPECT_FLOAT_EQ(x, 1920.0F);
    EXPECT_FLOAT_EQ(y, 1080.0F);
}

TEST(UiAnchorResolution, BottomRightCornerWithPivot) {
    const sq::ui::UiAnchor anchor{.anchorX = 1.0F, .anchorY = 1.0F, .pivotX = 1.0F, .pivotY = 1.0F};
    auto [x, y] = resolveAnchor(anchor, 1920.0F, 1080.0F, 200.0F, 40.0F);

    EXPECT_FLOAT_EQ(x, 1920.0F - 200.0F);
    EXPECT_FLOAT_EQ(y, 1080.0F - 40.0F);
}

TEST(UiAnchorResolution, CenterWithOffset) {
    const sq::ui::UiAnchor anchor{.anchorX = 0.5F, .anchorY = 0.5F, .offsetX = -50.0F, .offsetY = 10.0F};
    auto [x, y] = resolveAnchor(anchor, 1920.0F, 1080.0F);

    EXPECT_FLOAT_EQ(x, 910.0F);
    EXPECT_FLOAT_EQ(y, 550.0F);
}

TEST(UiAnchorResolution, CenterPivotCentersElement) {
    const sq::ui::UiAnchor anchor{.anchorX = 0.5F, .anchorY = 0.5F, .pivotX = 0.5F, .pivotY = 0.5F};
    auto [x, y] = resolveAnchor(anchor, 1920.0F, 1080.0F, 200.0F, 40.0F);

    EXPECT_FLOAT_EQ(x, 960.0F - 100.0F);
    EXPECT_FLOAT_EQ(y, 540.0F - 20.0F);
}

TEST(UiAnchor, DefaultPivotIsZero) {
    const sq::ui::UiAnchor anchor;

    EXPECT_EQ(anchor.pivotX, 0.0F);
    EXPECT_EQ(anchor.pivotY, 0.0F);
}

TEST(UiRect, DefaultValues) {
    const sq::ui::UiRect rect;

    EXPECT_EQ(rect.width, 0.0F);
    EXPECT_EQ(rect.height, 0.0F);
    EXPECT_EQ(rect.borderThickness, 0.0F);
    EXPECT_EQ(rect.borderColor.a, 0);
}

TEST(UiRect, DesignatedInitializersKeepOtherDefaults) {
    const sq::ui::UiRect rect{
        .width = 200.0F,
        .height = 40.0F,
        .fillColor = {.r = 255, .g = 0, .b = 0, .a = 255},
    };

    EXPECT_EQ(rect.width, 200.0F);
    EXPECT_EQ(rect.height, 40.0F);
    EXPECT_EQ(rect.fillColor.r, 255);
    EXPECT_EQ(rect.borderThickness, 0.0F);
}

TEST(UiText, DefaultValues) {
    const sq::ui::UiText text;

    EXPECT_TRUE(text.text.empty());
    EXPECT_EQ(text.fontSize, 20.0F);
    EXPECT_EQ(text.fontId, sq::ui::NO_FONT);
}

TEST(UiText, DesignatedInitializersKeepOtherDefaults) {
    const sq::ui::UiText text{.text = "score: 0", .fontSize = 32.0F};

    EXPECT_EQ(text.text, "score: 0");
    EXPECT_EQ(text.fontSize, 32.0F);
    EXPECT_EQ(text.fontId, sq::ui::NO_FONT);
}

TEST(UiProgressBar, DefaultValues) {
    const sq::ui::UiProgressBar bar;

    EXPECT_EQ(bar.value, 0.0F);
    EXPECT_EQ(bar.min, 0.0F);
    EXPECT_EQ(bar.max, 1.0F);
    EXPECT_EQ(bar.width, 100.0F);
    EXPECT_EQ(bar.height, 16.0F);
}

TEST(UiProgressBar, DesignatedInitializersKeepOtherDefaults) {
    const sq::ui::UiProgressBar bar{.value = 50.0F, .min = 0.0F, .max = 100.0F};

    EXPECT_EQ(bar.value, 50.0F);
    EXPECT_EQ(bar.max, 100.0F);
    EXPECT_EQ(bar.width, 100.0F);
}

static float computeRatio(const sq::ui::UiProgressBar& bar) {
    const float range = bar.max - bar.min;
    if (range <= 0.0F) {
        return 0.0F;
    }
    return std::clamp((bar.value - bar.min) / range, 0.0F, 1.0F);
}

TEST(UiProgressBarRatio, MidRange) {
    const sq::ui::UiProgressBar bar{.value = 50.0F, .min = 0.0F, .max = 100.0F};
    EXPECT_FLOAT_EQ(computeRatio(bar), 0.5F);
}

TEST(UiProgressBarRatio, ClampsBelowMin) {
    const sq::ui::UiProgressBar bar{.value = -10.0F, .min = 0.0F, .max = 100.0F};
    EXPECT_FLOAT_EQ(computeRatio(bar), 0.0F);
}

TEST(UiProgressBarRatio, ClampsAboveMax) {
    const sq::ui::UiProgressBar bar{.value = 150.0F, .min = 0.0F, .max = 100.0F};
    EXPECT_FLOAT_EQ(computeRatio(bar), 1.0F);
}

TEST(UiProgressBarRatio, ZeroRangeReturnsZero) {
    const sq::ui::UiProgressBar bar{.value = 5.0F, .min = 5.0F, .max = 5.0F};
    EXPECT_FLOAT_EQ(computeRatio(bar), 0.0F);
}

TEST(UiButton, DefaultValues) {
    const sq::ui::UiButton btn;

    EXPECT_EQ(btn.state, sq::ui::UiButtonState::Idle);
    EXPECT_EQ(btn.width, 120.0F);
    EXPECT_EQ(btn.height, 36.0F);
    EXPECT_TRUE(btn.label.empty());
    EXPECT_TRUE(btn.action.empty());
}

TEST(UiButton, WorksAsEcsComponent) {
    sq::ecs::Registry registry;
    const sq::ecs::Entity entity = registry.spawnEntity();

    registry.addComponent<sq::ui::UiButton>(entity, sq::ui::UiButton{.action = "fire"});

    ASSERT_TRUE(registry.hasComponent<sq::ui::UiButton>(entity));
    EXPECT_EQ(registry.getComponents<sq::ui::UiButton>().get(entity).action, "fire");
}

TEST(UiButtonSystem, IdleToHoveredWhenMouseEnters) {
    sq::ecs::Registry reg;
    sq::events::EventBus bus;
    FakeWindow win;
    const sq::ecs::Entity e = makeButton(reg, 0.0F, 0.0F);

    win.mx = 60.0F;
    win.my = 18.0F;
    updateButtons(reg, win, bus);

    EXPECT_EQ(reg.getComponents<sq::ui::UiButton>().get(e).state, sq::ui::UiButtonState::Hovered);
}

TEST(UiButtonSystem, HoveredToIdleWhenMouseLeaves) {
    sq::ecs::Registry reg;
    sq::events::EventBus bus;
    FakeWindow win;
    const sq::ecs::Entity e = makeButton(reg, 0.0F, 0.0F);
    reg.getComponents<sq::ui::UiButton>().get(e).state = sq::ui::UiButtonState::Hovered;

    win.mx = 999.0F;
    win.my = 999.0F;
    updateButtons(reg, win, bus);

    EXPECT_EQ(reg.getComponents<sq::ui::UiButton>().get(e).state, sq::ui::UiButtonState::Idle);
}

TEST(UiButtonSystem, HoveredToPressedOnMouseDown) {
    sq::ecs::Registry reg;
    sq::events::EventBus bus;
    FakeWindow win;
    const sq::ecs::Entity e = makeButton(reg, 0.0F, 0.0F);
    reg.getComponents<sq::ui::UiButton>().get(e).state = sq::ui::UiButtonState::Hovered;

    win.mx = 60.0F;
    win.my = 18.0F;
    win.press();
    updateButtons(reg, win, bus);

    EXPECT_EQ(reg.getComponents<sq::ui::UiButton>().get(e).state, sq::ui::UiButtonState::Pressed);
}

TEST(UiButtonSystem, IdleToPressedWhenEnteringAndPressingSameFrame) {
    sq::ecs::Registry reg;
    sq::events::EventBus bus;
    FakeWindow win;
    const sq::ecs::Entity e = makeButton(reg, 0.0F, 0.0F);

    win.mx = 60.0F;
    win.my = 18.0F;
    win.press();
    updateButtons(reg, win, bus);

    EXPECT_EQ(reg.getComponents<sq::ui::UiButton>().get(e).state, sq::ui::UiButtonState::Pressed);
}

TEST(UiButtonSystem, HeldMouseDraggedOntoButtonDoesNotPress) {
    sq::ecs::Registry reg;
    sq::events::EventBus bus;
    FakeWindow win;
    const sq::ecs::Entity e = makeButton(reg, 0.0F, 0.0F);
    reg.getComponents<sq::ui::UiButton>().get(e).state = sq::ui::UiButtonState::Hovered;

    win.mx = 60.0F;
    win.my = 18.0F;
    win.leftDown = true;
    updateButtons(reg, win, bus);

    EXPECT_EQ(reg.getComponents<sq::ui::UiButton>().get(e).state, sq::ui::UiButtonState::Hovered);
}

TEST(UiButtonSystem, PressedToReleasedAndEventOnReleaseOverButton) {
    sq::ecs::Registry reg;
    sq::events::EventBus bus;
    FakeWindow win;
    const sq::ecs::Entity e = makeButton(reg, 0.0F, 0.0F, 120.0F, 36.0F, "shoot");
    reg.getComponents<sq::ui::UiButton>().get(e).state = sq::ui::UiButtonState::Pressed;

    std::string capturedAction;
    bus.subscribe<sq::ui::UiButtonClickedEvent>(
        [&](const sq::ui::UiButtonClickedEvent& ev) { capturedAction = ev.action; });

    win.mx = 60.0F;
    win.my = 18.0F;
    win.release();
    updateButtons(reg, win, bus);

    EXPECT_EQ(reg.getComponents<sq::ui::UiButton>().get(e).state, sq::ui::UiButtonState::Released);
    EXPECT_EQ(capturedAction, "shoot");
}

TEST(UiButtonSystem, ReleasedLastsOneFrame) {
    sq::ecs::Registry reg;
    sq::events::EventBus bus;
    FakeWindow win;
    const sq::ecs::Entity e = makeButton(reg, 0.0F, 0.0F);
    reg.getComponents<sq::ui::UiButton>().get(e).state = sq::ui::UiButtonState::Released;

    int clicks = 0;
    bus.subscribe<sq::ui::UiButtonClickedEvent>([&](const sq::ui::UiButtonClickedEvent&) { ++clicks; });

    win.mx = 60.0F;
    win.my = 18.0F;
    updateButtons(reg, win, bus);

    EXPECT_EQ(reg.getComponents<sq::ui::UiButton>().get(e).state, sq::ui::UiButtonState::Hovered);
    EXPECT_EQ(clicks, 0);
}

TEST(UiButtonSystem, ReleaseOutsideCancelsClick) {
    sq::ecs::Registry reg;
    sq::events::EventBus bus;
    FakeWindow win;
    const sq::ecs::Entity e = makeButton(reg, 0.0F, 0.0F, 120.0F, 36.0F, "quit");
    reg.getComponents<sq::ui::UiButton>().get(e).state = sq::ui::UiButtonState::Pressed;

    bool fired = false;
    bus.subscribe<sq::ui::UiButtonClickedEvent>([&](const sq::ui::UiButtonClickedEvent&) { fired = true; });

    win.mx = 999.0F;
    win.my = 999.0F;
    win.release();
    updateButtons(reg, win, bus);

    EXPECT_EQ(reg.getComponents<sq::ui::UiButton>().get(e).state, sq::ui::UiButtonState::Idle);
    EXPECT_FALSE(fired);
}

TEST(UiButtonSystem, NoEventWhenStillPressed) {
    sq::ecs::Registry reg;
    sq::events::EventBus bus;
    FakeWindow win;
    const sq::ecs::Entity e = makeButton(reg, 0.0F, 0.0F);
    reg.getComponents<sq::ui::UiButton>().get(e).state = sq::ui::UiButtonState::Pressed;

    bool fired = false;
    bus.subscribe<sq::ui::UiButtonClickedEvent>([&](const sq::ui::UiButtonClickedEvent&) { fired = true; });

    win.mx = 60.0F;
    win.my = 18.0F;
    win.leftDown = true;
    updateButtons(reg, win, bus);

    EXPECT_FALSE(fired);
    EXPECT_EQ(reg.getComponents<sq::ui::UiButton>().get(e).state, sq::ui::UiButtonState::Pressed);
}

TEST(UiButtonSystem, FullClickSequence) {
    sq::ecs::Registry reg;
    sq::events::EventBus bus;
    FakeWindow win;
    const sq::ecs::Entity e = makeButton(reg, 0.0F, 0.0F);
    int clicks = 0;
    bus.subscribe<sq::ui::UiButtonClickedEvent>([&](const sq::ui::UiButtonClickedEvent&) { ++clicks; });
    const auto state = [&] { return reg.getComponents<sq::ui::UiButton>().get(e).state; };

    win.mx = 60.0F;
    win.my = 18.0F;
    updateButtons(reg, win, bus);
    EXPECT_EQ(state(), sq::ui::UiButtonState::Hovered);

    win.press();
    updateButtons(reg, win, bus);
    EXPECT_EQ(state(), sq::ui::UiButtonState::Pressed);

    win.leftPressed = false;  // still held on the next frame
    updateButtons(reg, win, bus);
    EXPECT_EQ(state(), sq::ui::UiButtonState::Pressed);

    win.release();
    updateButtons(reg, win, bus);
    EXPECT_EQ(state(), sq::ui::UiButtonState::Released);

    win.leftReleased = false;
    updateButtons(reg, win, bus);
    EXPECT_EQ(state(), sq::ui::UiButtonState::Hovered);
    EXPECT_EQ(clicks, 1);
}

// --- Layout with entity hierarchies ---

TEST(UiLayout, ResolveAnchorIsRelativeToContainer) {
    const sq::ui::UiAnchor anchor{.anchorX = 0.5F, .anchorY = 1.0F, .offsetX = 5.0F, .pivotX = 0.5F, .pivotY = 1.0F};
    const sq::ui::UiScreenRect rect =
        sq::ui::resolveAnchor(anchor, sq::ui::UiScreenRect{.x = 100.0F, .y = 50.0F, .width = 200.0F, .height = 80.0F},
                              sq::ui::UiSize{.width = 40.0F, .height = 20.0F});

    EXPECT_FLOAT_EQ(rect.x, 100.0F + 100.0F + 5.0F - 20.0F);
    EXPECT_FLOAT_EQ(rect.y, 50.0F + 80.0F - 20.0F);
    EXPECT_FLOAT_EQ(rect.width, 40.0F);
    EXPECT_FLOAT_EQ(rect.height, 20.0F);
}

TEST(UiLayout, ElementSizeComesFromDrawable) {
    sq::ecs::Registry reg;
    const sq::ecs::Entity rect = reg.spawnEntity();
    reg.addComponent<sq::ui::UiRect>(rect, sq::ui::UiRect{.width = 30.0F, .height = 12.0F});
    const sq::ecs::Entity text = reg.spawnEntity();
    reg.addComponent<sq::ui::UiText>(text, sq::ui::UiText{.text = "hi"});
    const sq::ecs::Entity group = reg.spawnEntity();

    EXPECT_FLOAT_EQ(sq::ui::elementSize(reg, rect, fakeMeasure).width, 30.0F);
    EXPECT_FLOAT_EQ(sq::ui::elementSize(reg, text, fakeMeasure).height, FAKE_TEXT_SIZE.height);
    EXPECT_FLOAT_EQ(sq::ui::elementSize(reg, group, fakeMeasure).width, 0.0F);
}

TEST(UiLayout, ChildIsAnchoredInsideParentRect) {
    sq::ecs::Registry reg;
    const sq::ecs::Entity panel = reg.spawnEntity();
    reg.addComponent<sq::ui::UiAnchor>(panel, sq::ui::UiAnchor{.anchorX = 0.5F, .anchorY = 0.5F});
    reg.addComponent<sq::ui::UiRect>(panel, sq::ui::UiRect{.width = 400.0F, .height = 200.0F});
    const sq::ecs::Entity button = makeButton(reg, 1.0F, 1.0F, 100.0F, 40.0F);
    reg.getComponents<sq::ui::UiAnchor>().get(button).pivotX = 1.0F;
    reg.getComponents<sq::ui::UiAnchor>().get(button).pivotY = 1.0F;
    ASSERT_TRUE(sq::hierarchy::setParent(reg, button, panel));

    const auto nodes = layoutOf(reg, FakeWindow{});
    const sq::ui::UiLayoutNode* node = findNode(nodes, button);

    ASSERT_NE(node, nullptr);
    // Panel at (960, 540), 400x200: the button sits in its bottom-right corner.
    EXPECT_FLOAT_EQ(node->rect.x, 960.0F + 400.0F - 100.0F);
    EXPECT_FLOAT_EQ(node->rect.y, 540.0F + 200.0F - 40.0F);
}

TEST(UiLayout, NestingAccumulatesThroughSeveralLevels) {
    sq::ecs::Registry reg;
    const sq::ecs::Entity root = reg.spawnEntity();
    reg.addComponent<sq::ui::UiAnchor>(root, sq::ui::UiAnchor{.offsetX = 10.0F, .offsetY = 10.0F});
    const sq::ecs::Entity mid = reg.spawnEntity();
    reg.addComponent<sq::ui::UiAnchor>(mid, sq::ui::UiAnchor{.offsetX = 20.0F});
    const sq::ecs::Entity leaf = reg.spawnEntity();
    reg.addComponent<sq::ui::UiAnchor>(leaf, sq::ui::UiAnchor{.offsetY = 5.0F});
    ASSERT_TRUE(sq::hierarchy::setParent(reg, mid, root));
    ASSERT_TRUE(sq::hierarchy::setParent(reg, leaf, mid));

    const auto nodes = layoutOf(reg, FakeWindow{});
    const sq::ui::UiLayoutNode* node = findNode(nodes, leaf);

    ASSERT_NE(node, nullptr);
    EXPECT_FLOAT_EQ(node->rect.x, 30.0F);
    EXPECT_FLOAT_EQ(node->rect.y, 15.0F);
}

TEST(UiLayout, ParentComesBeforeChildrenInSiblingOrder) {
    sq::ecs::Registry reg;
    // Children spawned before the parent, so storage order alone would put them first.
    const sq::ecs::Entity second = makeButton(reg, 0.0F, 0.0F);
    const sq::ecs::Entity first = makeButton(reg, 0.0F, 0.0F);
    const sq::ecs::Entity panel = reg.spawnEntity();
    reg.addComponent<sq::ui::UiAnchor>(panel, sq::ui::UiAnchor{});
    ASSERT_TRUE(sq::hierarchy::setParent(reg, first, panel));
    ASSERT_TRUE(sq::hierarchy::setParent(reg, second, panel));

    const auto nodes = layoutOf(reg, FakeWindow{});

    ASSERT_EQ(nodes.size(), 3U);
    EXPECT_LT(indexOf(nodes, panel), indexOf(nodes, first));
    EXPECT_LT(indexOf(nodes, first), indexOf(nodes, second));
}

TEST(UiLayout, ChildOfNonUiEntityIsAnchoredToScreen) {
    sq::ecs::Registry reg;
    const sq::ecs::Entity ship = reg.spawnEntity();
    const sq::ecs::Entity label = makeButton(reg, 0.5F, 0.0F);
    ASSERT_TRUE(sq::hierarchy::setParent(reg, label, ship));

    const auto nodes = layoutOf(reg, FakeWindow{});
    const sq::ui::UiLayoutNode* node = findNode(nodes, label);

    ASSERT_NE(node, nullptr);
    EXPECT_FLOAT_EQ(node->rect.x, 960.0F);
}

TEST(UiLayout, ElementUnderNonUiChildIsAnchoredToScreen) {
    sq::ecs::Registry reg;
    const sq::ecs::Entity panel = reg.spawnEntity();
    reg.addComponent<sq::ui::UiAnchor>(panel, sq::ui::UiAnchor{.offsetX = 300.0F});
    const sq::ecs::Entity plain = reg.spawnEntity();
    const sq::ecs::Entity deep = makeButton(reg, 0.0F, 0.0F);
    ASSERT_TRUE(sq::hierarchy::setParent(reg, plain, panel));
    ASSERT_TRUE(sq::hierarchy::setParent(reg, deep, plain));

    const auto nodes = layoutOf(reg, FakeWindow{});
    const sq::ui::UiLayoutNode* node = findNode(nodes, deep);

    EXPECT_EQ(findNode(nodes, plain), nullptr);
    ASSERT_NE(node, nullptr);
    EXPECT_FLOAT_EQ(node->rect.x, 0.0F);
    EXPECT_EQ(std::ranges::count(nodes, deep, &sq::ui::UiLayoutNode::entity), 1);
}

TEST(UiLayout, DestroyingPanelRemovesItsWidgets) {
    sq::ecs::Registry reg;
    const sq::hierarchy::HierarchyHooks hooks(reg);
    const sq::ecs::Entity panel = reg.spawnEntity();
    reg.addComponent<sq::ui::UiAnchor>(panel, sq::ui::UiAnchor{});
    const sq::ecs::Entity button = makeButton(reg, 0.0F, 0.0F);
    ASSERT_TRUE(sq::hierarchy::setParent(reg, button, panel));

    reg.killEntity(panel);

    EXPECT_TRUE(layoutOf(reg, FakeWindow{}).empty());
}

TEST(UiButtonSystem, NestedButtonIsHitWhereItIsDrawn) {
    sq::ecs::Registry reg;
    sq::events::EventBus bus;
    FakeWindow win;
    const sq::ecs::Entity panel = reg.spawnEntity();
    reg.addComponent<sq::ui::UiAnchor>(panel, sq::ui::UiAnchor{.offsetX = 500.0F, .offsetY = 300.0F});
    const sq::ecs::Entity button = makeButton(reg, 0.0F, 0.0F);
    ASSERT_TRUE(sq::hierarchy::setParent(reg, button, panel));

    win.mx = 10.0F;
    win.my = 10.0F;
    updateButtons(reg, win, bus);
    EXPECT_EQ(reg.getComponents<sq::ui::UiButton>().get(button).state, sq::ui::UiButtonState::Idle);

    win.mx = 510.0F;
    win.my = 310.0F;
    updateButtons(reg, win, bus);
    EXPECT_EQ(reg.getComponents<sq::ui::UiButton>().get(button).state, sq::ui::UiButtonState::Hovered);
}

// --- Rendering through IScreenDrawService ---

TEST(UiRenderSystem, ParentIsSubmittedBeforeChild) {
    sq::ecs::Registry reg;
    constexpr sq::ecs::Rgba PARENT_COLOR = {.r = 10, .g = 0, .b = 0, .a = 255};
    constexpr sq::ecs::Rgba CHILD_COLOR = {.r = 20, .g = 0, .b = 0, .a = 255};
    // Child spawned first, so storage order alone would draw it below its parent.
    const sq::ecs::Entity child = reg.spawnEntity();
    reg.addComponent<sq::ui::UiAnchor>(child, sq::ui::UiAnchor{.offsetX = 5.0F});
    reg.addComponent<sq::ui::UiRect>(child, sq::ui::UiRect{.width = 10.0F, .height = 10.0F, .fillColor = CHILD_COLOR});
    const sq::ecs::Entity parent = reg.spawnEntity();
    reg.addComponent<sq::ui::UiAnchor>(parent, sq::ui::UiAnchor{.offsetX = 100.0F});
    reg.addComponent<sq::ui::UiRect>(parent,
                                     sq::ui::UiRect{.width = 50.0F, .height = 50.0F, .fillColor = PARENT_COLOR});
    ASSERT_TRUE(sq::hierarchy::setParent(reg, child, parent));

    FakeScreenDraw draw;
    renderUi(reg, draw);

    ASSERT_EQ(draw.calls.size(), 2U);
    EXPECT_EQ(draw.calls[0].color.r, PARENT_COLOR.r);
    EXPECT_EQ(draw.calls[1].color.r, CHILD_COLOR.r);
    EXPECT_FLOAT_EQ(draw.calls[1].rect.x, 105.0F);
}

TEST(UiRenderSystem, TransparentFillIsSkippedAndBorderDrawn) {
    sq::ecs::Registry reg;
    const sq::ecs::Entity e = reg.spawnEntity();
    reg.addComponent<sq::ui::UiAnchor>(e, sq::ui::UiAnchor{});
    reg.addComponent<sq::ui::UiRect>(e, sq::ui::UiRect{
                                            .width = 10.0F,
                                            .height = 10.0F,
                                            .fillColor = {.r = 0, .g = 0, .b = 0, .a = 0},
                                            .borderColor = {.r = 255, .g = 255, .b = 255, .a = 255},
                                            .borderThickness = 2.0F,
                                        });

    FakeScreenDraw draw;
    renderUi(reg, draw);

    ASSERT_EQ(draw.calls.size(), 1U);
    EXPECT_EQ(draw.calls[0].kind, "stroke");
}

TEST(UiRenderSystem, ProgressBarFillsBackgroundThenRatio) {
    sq::ecs::Registry reg;
    const sq::ecs::Entity e = reg.spawnEntity();
    reg.addComponent<sq::ui::UiAnchor>(e, sq::ui::UiAnchor{});
    reg.addComponent<sq::ui::UiProgressBar>(
        e, sq::ui::UiProgressBar{.value = 25.0F, .min = 0.0F, .max = 100.0F, .width = 200.0F, .height = 10.0F});

    FakeScreenDraw draw;
    renderUi(reg, draw);

    ASSERT_EQ(draw.calls.size(), 2U);
    EXPECT_FLOAT_EQ(draw.calls[0].rect.width, 200.0F);
    EXPECT_FLOAT_EQ(draw.calls[1].rect.width, 50.0F);
}

TEST(UiRenderSystem, ButtonLabelIsCentred) {
    sq::ecs::Registry reg;
    const sq::ecs::Entity e = makeButton(reg, 0.0F, 0.0F, 120.0F, 36.0F);
    reg.getComponents<sq::ui::UiButton>().get(e).label = "Play";

    FakeScreenDraw draw;
    renderUi(reg, draw);

    ASSERT_EQ(draw.calls.size(), 2U);
    EXPECT_EQ(draw.calls[0].kind, "fill");
    EXPECT_EQ(draw.calls[1].text, "Play");
    EXPECT_FLOAT_EQ(draw.calls[1].rect.x, (120.0F - FAKE_TEXT_SIZE.width) * 0.5F);
    EXPECT_FLOAT_EQ(draw.calls[1].rect.y, (36.0F - FAKE_TEXT_SIZE.height) * 0.5F);
}

TEST(UiRenderSystem, TextIsAnchoredWithMeasuredSize) {
    sq::ecs::Registry reg;
    const sq::ecs::Entity e = reg.spawnEntity();
    reg.addComponent<sq::ui::UiAnchor>(e, sq::ui::UiAnchor{.anchorX = 1.0F, .pivotX = 1.0F});
    reg.addComponent<sq::ui::UiText>(e, sq::ui::UiText{.text = "SCORE"});

    FakeScreenDraw draw;
    renderUi(reg, draw);

    ASSERT_EQ(draw.calls.size(), 1U);
    EXPECT_EQ(draw.calls[0].text, "SCORE");
    EXPECT_FLOAT_EQ(draw.calls[0].rect.x, 1920.0F - FAKE_TEXT_SIZE.width);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
