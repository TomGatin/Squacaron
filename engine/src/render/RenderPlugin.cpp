/**
 * @file RenderPlugin.cpp
 * @brief Implementation of RenderPlugin and its abstract service adapters.
 */

#include "render/RenderPlugin.hpp"

#include <raylib.h>

#include <bitset>
#include <cstddef>
#include <memory>
#include <utility>

#include "core/Engine.hpp"
#include "core/Mediator.hpp"
#include "core/MediatorError.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "render/api/IInputCaptureService.hpp"
#include "render/api/IRenderService.hpp"
#include "render/api/IScreenDrawService.hpp"
#include "render/api/IWindowService.hpp"
#include "render/api/InputCodes.hpp"
#include "render/screen/ScreenDrawService.hpp"
#include "render/texture/TextureStore.hpp"
#include "render/window/Window.hpp"
#include "render/window/WindowConfig.hpp"
#include "systems/render/RenderSystem.hpp"

namespace sq::render {

namespace {

/** @brief Raylib has no enumerator for an absent gamepad axis. */
constexpr int INVALID_GAMEPAD_AXIS = -1;

/** @brief Raylib has no enumerator for an unknown mouse button. */
constexpr int INVALID_MOUSE_BUTTON = -1;

}  // namespace

namespace {

[[nodiscard]] int toRaylib(sq::engine::Key key) noexcept {
    switch (key) {
        case sq::engine::Key::Unknown:
            return KEY_NULL;
        case sq::engine::Key::Apostrophe:
            return KEY_APOSTROPHE;
        case sq::engine::Key::Comma:
            return KEY_COMMA;
        case sq::engine::Key::Minus:
            return KEY_MINUS;
        case sq::engine::Key::Period:
            return KEY_PERIOD;
        case sq::engine::Key::Slash:
            return KEY_SLASH;
        case sq::engine::Key::Digit0:
            return KEY_ZERO;
        case sq::engine::Key::Digit1:
            return KEY_ONE;
        case sq::engine::Key::Digit2:
            return KEY_TWO;
        case sq::engine::Key::Digit3:
            return KEY_THREE;
        case sq::engine::Key::Digit4:
            return KEY_FOUR;
        case sq::engine::Key::Digit5:
            return KEY_FIVE;
        case sq::engine::Key::Digit6:
            return KEY_SIX;
        case sq::engine::Key::Digit7:
            return KEY_SEVEN;
        case sq::engine::Key::Digit8:
            return KEY_EIGHT;
        case sq::engine::Key::Digit9:
            return KEY_NINE;
        case sq::engine::Key::Semicolon:
            return KEY_SEMICOLON;
        case sq::engine::Key::Equal:
            return KEY_EQUAL;
        case sq::engine::Key::A:
            return KEY_A;
        case sq::engine::Key::B:
            return KEY_B;
        case sq::engine::Key::C:
            return KEY_C;
        case sq::engine::Key::D:
            return KEY_D;
        case sq::engine::Key::E:
            return KEY_E;
        case sq::engine::Key::F:
            return KEY_F;
        case sq::engine::Key::G:
            return KEY_G;
        case sq::engine::Key::H:
            return KEY_H;
        case sq::engine::Key::I:
            return KEY_I;
        case sq::engine::Key::J:
            return KEY_J;
        case sq::engine::Key::K:
            return KEY_K;
        case sq::engine::Key::L:
            return KEY_L;
        case sq::engine::Key::M:
            return KEY_M;
        case sq::engine::Key::N:
            return KEY_N;
        case sq::engine::Key::O:
            return KEY_O;
        case sq::engine::Key::P:
            return KEY_P;
        case sq::engine::Key::Q:
            return KEY_Q;
        case sq::engine::Key::R:
            return KEY_R;
        case sq::engine::Key::S:
            return KEY_S;
        case sq::engine::Key::T:
            return KEY_T;
        case sq::engine::Key::U:
            return KEY_U;
        case sq::engine::Key::V:
            return KEY_V;
        case sq::engine::Key::W:
            return KEY_W;
        case sq::engine::Key::X:
            return KEY_X;
        case sq::engine::Key::Y:
            return KEY_Y;
        case sq::engine::Key::Z:
            return KEY_Z;
        case sq::engine::Key::LeftBracket:
            return KEY_LEFT_BRACKET;
        case sq::engine::Key::Backslash:
            return KEY_BACKSLASH;
        case sq::engine::Key::RightBracket:
            return KEY_RIGHT_BRACKET;
        case sq::engine::Key::Grave:
            return KEY_GRAVE;
        case sq::engine::Key::Space:
            return KEY_SPACE;
        case sq::engine::Key::Escape:
            return KEY_ESCAPE;
        case sq::engine::Key::Enter:
            return KEY_ENTER;
        case sq::engine::Key::Tab:
            return KEY_TAB;
        case sq::engine::Key::Backspace:
            return KEY_BACKSPACE;
        case sq::engine::Key::Insert:
            return KEY_INSERT;
        case sq::engine::Key::Delete:
            return KEY_DELETE;
        case sq::engine::Key::Right:
            return KEY_RIGHT;
        case sq::engine::Key::Left:
            return KEY_LEFT;
        case sq::engine::Key::Down:
            return KEY_DOWN;
        case sq::engine::Key::Up:
            return KEY_UP;
        case sq::engine::Key::PageUp:
            return KEY_PAGE_UP;
        case sq::engine::Key::PageDown:
            return KEY_PAGE_DOWN;
        case sq::engine::Key::Home:
            return KEY_HOME;
        case sq::engine::Key::End:
            return KEY_END;
        case sq::engine::Key::CapsLock:
            return KEY_CAPS_LOCK;
        case sq::engine::Key::ScrollLock:
            return KEY_SCROLL_LOCK;
        case sq::engine::Key::NumLock:
            return KEY_NUM_LOCK;
        case sq::engine::Key::PrintScreen:
            return KEY_PRINT_SCREEN;
        case sq::engine::Key::Pause:
            return KEY_PAUSE;
        case sq::engine::Key::F1:
            return KEY_F1;
        case sq::engine::Key::F2:
            return KEY_F2;
        case sq::engine::Key::F3:
            return KEY_F3;
        case sq::engine::Key::F4:
            return KEY_F4;
        case sq::engine::Key::F5:
            return KEY_F5;
        case sq::engine::Key::F6:
            return KEY_F6;
        case sq::engine::Key::F7:
            return KEY_F7;
        case sq::engine::Key::F8:
            return KEY_F8;
        case sq::engine::Key::F9:
            return KEY_F9;
        case sq::engine::Key::F10:
            return KEY_F10;
        case sq::engine::Key::F11:
            return KEY_F11;
        case sq::engine::Key::F12:
            return KEY_F12;
        case sq::engine::Key::LeftShift:
            return KEY_LEFT_SHIFT;
        case sq::engine::Key::LeftControl:
            return KEY_LEFT_CONTROL;
        case sq::engine::Key::LeftAlt:
            return KEY_LEFT_ALT;
        case sq::engine::Key::LeftSuper:
            return KEY_LEFT_SUPER;
        case sq::engine::Key::RightShift:
            return KEY_RIGHT_SHIFT;
        case sq::engine::Key::RightControl:
            return KEY_RIGHT_CONTROL;
        case sq::engine::Key::RightAlt:
            return KEY_RIGHT_ALT;
        case sq::engine::Key::RightSuper:
            return KEY_RIGHT_SUPER;
        case sq::engine::Key::KeyboardMenu:
            return KEY_KB_MENU;
        case sq::engine::Key::Keypad0:
            return KEY_KP_0;
        case sq::engine::Key::Keypad1:
            return KEY_KP_1;
        case sq::engine::Key::Keypad2:
            return KEY_KP_2;
        case sq::engine::Key::Keypad3:
            return KEY_KP_3;
        case sq::engine::Key::Keypad4:
            return KEY_KP_4;
        case sq::engine::Key::Keypad5:
            return KEY_KP_5;
        case sq::engine::Key::Keypad6:
            return KEY_KP_6;
        case sq::engine::Key::Keypad7:
            return KEY_KP_7;
        case sq::engine::Key::Keypad8:
            return KEY_KP_8;
        case sq::engine::Key::Keypad9:
            return KEY_KP_9;
        case sq::engine::Key::KeypadDecimal:
            return KEY_KP_DECIMAL;
        case sq::engine::Key::KeypadDivide:
            return KEY_KP_DIVIDE;
        case sq::engine::Key::KeypadMultiply:
            return KEY_KP_MULTIPLY;
        case sq::engine::Key::KeypadSubtract:
            return KEY_KP_SUBTRACT;
        case sq::engine::Key::KeypadAdd:
            return KEY_KP_ADD;
        case sq::engine::Key::KeypadEnter:
            return KEY_KP_ENTER;
        case sq::engine::Key::KeypadEqual:
            return KEY_KP_EQUAL;
    }
    return KEY_NULL;
}

[[nodiscard]] int toRaylib(sq::engine::GamepadButton button) noexcept {
    switch (button) {
        case sq::engine::GamepadButton::Unknown:
            return GAMEPAD_BUTTON_UNKNOWN;
        case sq::engine::GamepadButton::LeftFaceUp:
            return GAMEPAD_BUTTON_LEFT_FACE_UP;
        case sq::engine::GamepadButton::LeftFaceRight:
            return GAMEPAD_BUTTON_LEFT_FACE_RIGHT;
        case sq::engine::GamepadButton::LeftFaceDown:
            return GAMEPAD_BUTTON_LEFT_FACE_DOWN;
        case sq::engine::GamepadButton::LeftFaceLeft:
            return GAMEPAD_BUTTON_LEFT_FACE_LEFT;
        case sq::engine::GamepadButton::RightFaceUp:
            return GAMEPAD_BUTTON_RIGHT_FACE_UP;
        case sq::engine::GamepadButton::RightFaceRight:
            return GAMEPAD_BUTTON_RIGHT_FACE_RIGHT;
        case sq::engine::GamepadButton::RightFaceDown:
            return GAMEPAD_BUTTON_RIGHT_FACE_DOWN;
        case sq::engine::GamepadButton::RightFaceLeft:
            return GAMEPAD_BUTTON_RIGHT_FACE_LEFT;
        case sq::engine::GamepadButton::LeftTrigger1:
            return GAMEPAD_BUTTON_LEFT_TRIGGER_1;
        case sq::engine::GamepadButton::LeftTrigger2:
            return GAMEPAD_BUTTON_LEFT_TRIGGER_2;
        case sq::engine::GamepadButton::RightTrigger1:
            return GAMEPAD_BUTTON_RIGHT_TRIGGER_1;
        case sq::engine::GamepadButton::RightTrigger2:
            return GAMEPAD_BUTTON_RIGHT_TRIGGER_2;
        case sq::engine::GamepadButton::MiddleLeft:
            return GAMEPAD_BUTTON_MIDDLE_LEFT;
        case sq::engine::GamepadButton::Middle:
            return GAMEPAD_BUTTON_MIDDLE;
        case sq::engine::GamepadButton::MiddleRight:
            return GAMEPAD_BUTTON_MIDDLE_RIGHT;
        case sq::engine::GamepadButton::LeftThumb:
            return GAMEPAD_BUTTON_LEFT_THUMB;
        case sq::engine::GamepadButton::RightThumb:
            return GAMEPAD_BUTTON_RIGHT_THUMB;
    }
    return GAMEPAD_BUTTON_UNKNOWN;
}

[[nodiscard]] int toRaylib(sq::engine::MouseButton button) noexcept {
    switch (button) {
        case sq::engine::MouseButton::Unknown:
            return INVALID_MOUSE_BUTTON;
        case sq::engine::MouseButton::Left:
            return MOUSE_BUTTON_LEFT;
        case sq::engine::MouseButton::Right:
            return MOUSE_BUTTON_RIGHT;
        case sq::engine::MouseButton::Middle:
            return MOUSE_BUTTON_MIDDLE;
        case sq::engine::MouseButton::Side:
            return MOUSE_BUTTON_SIDE;
        case sq::engine::MouseButton::Extra:
            return MOUSE_BUTTON_EXTRA;
        case sq::engine::MouseButton::Forward:
            return MOUSE_BUTTON_FORWARD;
        case sq::engine::MouseButton::Back:
            return MOUSE_BUTTON_BACK;
    }
    return INVALID_MOUSE_BUTTON;
}

[[nodiscard]] int toRaylib(sq::engine::GamepadAxis axis) noexcept {
    switch (axis) {
        case sq::engine::GamepadAxis::None:
            return INVALID_GAMEPAD_AXIS;
        case sq::engine::GamepadAxis::LeftX:
            return GAMEPAD_AXIS_LEFT_X;
        case sq::engine::GamepadAxis::LeftY:
            return GAMEPAD_AXIS_LEFT_Y;
        case sq::engine::GamepadAxis::RightX:
            return GAMEPAD_AXIS_RIGHT_X;
        case sq::engine::GamepadAxis::RightY:
            return GAMEPAD_AXIS_RIGHT_Y;
        case sq::engine::GamepadAxis::LeftTrigger:
            return GAMEPAD_AXIS_LEFT_TRIGGER;
        case sq::engine::GamepadAxis::RightTrigger:
            return GAMEPAD_AXIS_RIGHT_TRIGGER;
    }
    return INVALID_GAMEPAD_AXIS;
}

/** @brief Number of `sq::engine::Key` slots: every value of its 8-bit underlying type. */
constexpr std::size_t KEY_SLOTS = 256;

}  // namespace

/**
 * @brief Concrete adapter implementing IWindowService via Raylib Window.
 *
 * While the input is captured (see `InputCaptureImpl`), every input query reports an idle device. Keys held when the
 * capture is released stay hidden until they are released.
 */
class RenderPlugin::WindowServiceImpl final : public sq::engine::IWindowService {
  public:
    [[nodiscard]] bool shouldClose() const noexcept override { return Window::shouldClose(); }

    void beginFrame() override { Window::beginFrame(); }

    void endFrame() override { Window::endFrame(); }

    // The real size of the window, which differs from the configured one once it is resized.
    [[nodiscard]] int width() const noexcept override { return GetScreenWidth(); }

    [[nodiscard]] int height() const noexcept override { return GetScreenHeight(); }

    [[nodiscard]] bool isKeyDown(sq::engine::Key key) const noexcept override {
        if (_captured) {
            return false;
        }
        const bool down = rawKeyDown(key);
        const auto index = static_cast<std::size_t>(key);
        if (!down) {
            _swallowed.reset(index);
        }
        return down && !_swallowed.test(index);
    }

    [[nodiscard]] bool isGamepadAvailable(int gamepad) const noexcept override { return IsGamepadAvailable(gamepad); }

    [[nodiscard]] bool isGamepadButtonDown(int gamepad, sq::engine::GamepadButton button) const noexcept override {
        const int code = toRaylib(button);
        return !_captured && code != GAMEPAD_BUTTON_UNKNOWN && IsGamepadButtonDown(gamepad, code);
    }

    [[nodiscard]] float gamepadAxis(int gamepad, sq::engine::GamepadAxis axis) const noexcept override {
        const int code = toRaylib(axis);
        return !_captured && code != INVALID_GAMEPAD_AXIS ? GetGamepadAxisMovement(gamepad, code) : 0.0F;
    }

    [[nodiscard]] bool isMouseButtonDown(sq::engine::MouseButton button) const noexcept override {
        const int code = toRaylib(button);
        return !_captured && code != INVALID_MOUSE_BUTTON && IsMouseButtonDown(code);
    }

    [[nodiscard]] bool isMouseButtonPressed(sq::engine::MouseButton button) const noexcept override {
        const int code = toRaylib(button);
        return !_captured && code != INVALID_MOUSE_BUTTON && IsMouseButtonPressed(code);
    }

    [[nodiscard]] bool isMouseButtonReleased(sq::engine::MouseButton button) const noexcept override {
        const int code = toRaylib(button);
        return !_captured && code != INVALID_MOUSE_BUTTON && IsMouseButtonReleased(code);
    }

    [[nodiscard]] sq::engine::MousePosition mousePosition() const noexcept override {
        const Vector2 position = GetMousePosition();
        return sq::engine::MousePosition{.x = position.x, .y = position.y};
    }

    [[nodiscard]] float mouseWheelDelta() const noexcept override { return _captured ? 0.0F : rawWheelDelta(); }

    [[nodiscard]] char32_t nextTypedCharacter() noexcept override { return _captured ? U'\0' : rawTypedCharacter(); }

    /** @brief Captures or releases the input; releasing hides the keys still held until they are released. */
    void setCaptured(bool captured) noexcept {
        if (_captured && !captured) {
            for (std::size_t index = 1; index < _swallowed.size(); ++index) {
                if (rawKeyDown(static_cast<sq::engine::Key>(index))) {
                    _swallowed.set(index);
                }
            }
        }
        _captured = captured;
    }

    /** @brief Tells whether the input is captured. */
    [[nodiscard]] bool isCaptured() const noexcept { return _captured; }

    /** @brief Reads a key, ignoring the capture. */
    [[nodiscard]] static bool rawKeyDown(sq::engine::Key key) noexcept {
        const int code = toRaylib(key);
        return code != KEY_NULL && IsKeyDown(code);
    }

    /** @brief Pops the next typed character, ignoring the capture. */
    [[nodiscard]] static char32_t rawTypedCharacter() noexcept {
        const int character = GetCharPressed();
        return character > 0 ? static_cast<char32_t>(character) : U'\0';
    }

    /** @brief Reads the mouse wheel, ignoring the capture. */
    [[nodiscard]] static float rawWheelDelta() noexcept { return GetMouseWheelMove(); }

  private:
    /** @brief Whether the input is hidden from the window service. */
    bool _captured = false;
    /** @brief Keys held when the capture was released, hidden until released (indexed by `sq::engine::Key`). */
    mutable std::bitset<KEY_SLOTS> _swallowed;
};

/**
 * @brief Concrete adapter implementing IInputCaptureService on top of the window service state.
 */
class RenderPlugin::InputCaptureImpl final : public sq::engine::IInputCaptureService {
  public:
    /**
     * @brief Creates the adapter.
     * @param window Window service whose input it captures.
     */
    explicit InputCaptureImpl(std::shared_ptr<WindowServiceImpl> window) noexcept : _window(std::move(window)) {}

    void setCaptured(bool captured) override { _window->setCaptured(captured); }

    [[nodiscard]] bool isCaptured() const noexcept override { return _window->isCaptured(); }

    [[nodiscard]] bool isKeyDown(sq::engine::Key key) const noexcept override {
        return WindowServiceImpl::rawKeyDown(key);
    }

    [[nodiscard]] char32_t nextTypedCharacter() noexcept override { return WindowServiceImpl::rawTypedCharacter(); }

    [[nodiscard]] float mouseWheelDelta() const noexcept override { return WindowServiceImpl::rawWheelDelta(); }

  private:
    /** @brief Window service whose input is captured. */
    std::shared_ptr<WindowServiceImpl> _window;
};

/**
 * @brief Concrete adapter implementing IRenderService via RenderSystem.
 */
class RenderPlugin::RenderServiceImpl final : public sq::engine::IRenderService {
  public:
    /**
     * @brief Constructs RenderServiceImpl wrapping a RenderSystem reference.
     * @param system Reference to the active RenderSystem.
     */
    explicit RenderServiceImpl(RenderSystem& system) : _system(&system) {}

    void collect(sq::ecs::Registry& registry) override {
        if (_system != nullptr) {
            _system->collect(registry);
        }
    }

    void draw() const override {
        if (_system != nullptr) {
            _system->draw();
        }
    }

    void update(sq::ecs::Registry& registry) override {
        if (_system != nullptr) {
            _system->update(registry);
        }
    }

    /**
     * @brief Sets or updates the wrapped RenderSystem pointer.
     * @param system Pointer to RenderSystem instance.
     */
    void setSystem(RenderSystem* system) noexcept { _system = system; }

  private:
    /** @brief Non-owning pointer to the underlying RenderSystem. */
    RenderSystem* _system = nullptr;
};

RenderPlugin::RenderPlugin(WindowConfig config) : _config(std::move(config)) {}

RenderPlugin::~RenderPlugin() {
    _screenDrawService.reset();
    _renderService.reset();
    _inputCapture.reset();
    _windowService.reset();
    _renderSystem.reset();
    _textures.reset();
    _window.reset();
}

void RenderPlugin::build(sq::engine::PluginBuilder& builder) {
    builder.require<sq::engine::CorePlugin>();
    builder.provides<sq::engine::IWindowService>();
    builder.provides<sq::engine::IInputCaptureService>();
    builder.provides<sq::engine::IRenderService>();
    builder.provides<sq::engine::IScreenDrawService>();
}

void RenderPlugin::init(sq::engine::Engine& engine) {
    _window = std::make_unique<Window>(_config);
    _textures = std::make_unique<TextureStore>();
    _renderSystem = std::make_unique<RenderSystem>(*_textures);

    _windowService = std::make_shared<WindowServiceImpl>();
    _inputCapture = std::make_shared<InputCaptureImpl>(_windowService);
    _renderService = std::make_shared<RenderServiceImpl>(*_renderSystem);
    _screenDrawService = std::make_shared<ScreenDrawService>(_textures.get());
    // The window waits for its target frame rate: Engine::run must not sleep on top of it.
    engine.setExternallyPaced(true);

    if (const auto res = engine.mediator().registerService<sq::engine::IWindowService>(_windowService); !res) {
        throw sq::engine::MediatorError(res.error());
    }
    if (const auto res = engine.mediator().registerService<sq::engine::IInputCaptureService>(_inputCapture); !res) {
        engine.mediator().removeService<sq::engine::IWindowService>();
        throw sq::engine::MediatorError(res.error());
    }
    if (const auto res = engine.mediator().registerService<sq::engine::IRenderService>(_renderService); !res) {
        engine.mediator().removeService<sq::engine::IInputCaptureService>();
        engine.mediator().removeService<sq::engine::IWindowService>();
        throw sq::engine::MediatorError(res.error());
    }
    if (const auto res = engine.mediator().registerService<sq::engine::IScreenDrawService>(_screenDrawService); !res) {
        engine.mediator().removeService<sq::engine::IRenderService>();
        engine.mediator().removeService<sq::engine::IInputCaptureService>();
        engine.mediator().removeService<sq::engine::IWindowService>();
        throw sq::engine::MediatorError(res.error());
    }
}

void RenderPlugin::render(sq::engine::Engine& engine) {
    Window::beginFrame();
    if (_renderSystem != nullptr) {
        _renderSystem->update(engine.registry());
    }
    // Overlays queued during update/lateUpdate go on top of the world, before the frame is presented.
    if (_screenDrawService != nullptr) {
        _screenDrawService->flush();
    }
    Window::endFrame();
    if (Window::shouldClose()) {
        engine.stop();
    }
}

void RenderPlugin::shutdown(sq::engine::Engine& engine) {
    if (engine.mediator().hasService<sq::engine::IScreenDrawService>()) {
        engine.mediator().removeService<sq::engine::IScreenDrawService>();
    }
    if (engine.mediator().hasService<sq::engine::IRenderService>()) {
        engine.mediator().removeService<sq::engine::IRenderService>();
    }
    if (engine.mediator().hasService<sq::engine::IInputCaptureService>()) {
        engine.mediator().removeService<sq::engine::IInputCaptureService>();
    }
    if (engine.mediator().hasService<sq::engine::IWindowService>()) {
        engine.mediator().removeService<sq::engine::IWindowService>();
    }

    if (_renderService != nullptr) {
        _renderService->setSystem(nullptr);
    }
    _screenDrawService.reset();
    _renderService.reset();
    _inputCapture.reset();
    _windowService.reset();
    _renderSystem.reset();
    _textures.reset();
    _window.reset();
}

}  // namespace sq::render
