#include "tools/devconsole/DevConsolePlugin.hpp"

#include <cmath>
#include <cstddef>
#include <memory>
#include <utility>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/time/Time.hpp"
#include "render/api/IInputCaptureService.hpp"
#include "render/api/IScreenDrawService.hpp"
#include "render/api/IWindowService.hpp"
#include "tools/console/Console.hpp"
#include "tools/console/ConsolePlugin.hpp"
#include "tools/devconsole/DevConsole.hpp"
#include "tools/devconsole/DevConsoleTypes.hpp"
#include "tools/devconsole/KeyRepeater.hpp"
#include "ui/UiPlugin.hpp"

namespace sq::tools {

DevConsolePlugin::DevConsolePlugin(DevConsoleConfig config)
    : _config(std::move(config)), _view(_config), _toggle(false, _config.repeatDelay, _config.repeatInterval) {
    _repeaters.reserve(KEY_BINDINGS.size());
    for (const KeyBinding& binding : KEY_BINDINGS) {
        _repeaters.emplace_back(binding.repeats, _config.repeatDelay, _config.repeatInterval);
    }
}

DevConsolePlugin::~DevConsolePlugin() = default;

void DevConsolePlugin::build(sq::engine::PluginBuilder& builder) {
    builder.require<ConsolePlugin>();
    builder.requireService<sq::engine::IWindowService>();
    builder.requireService<sq::engine::IInputCaptureService>();
    builder.requireService<sq::engine::IScreenDrawService>();
    // The console is drawn after the UI, so it stays on top of menus and overlays.
    builder.optionally<sq::ui::UiPlugin>();
}

void DevConsolePlugin::init(sq::engine::Engine& engine) {
    _devConsole = std::make_unique<DevConsole>(engine.service<Console>());
    if (!_config.fontPath.empty()) {
        _view.setFont(engine.service<sq::engine::IScreenDrawService>().loadFont(_config.fontPath));
    }
}

void DevConsolePlugin::preUpdate(sq::engine::Engine& engine) {
    auto found = engine.mediator().getService<sq::engine::IInputCaptureService>();
    if (!_devConsole || !found) {
        return;
    }
    sq::engine::IInputCaptureService& capture = found->get();
    const float dt = engine.clock().lastFrameDelta();

    const bool wasOpen = _devConsole->isOpen();
    if (_toggle.update(capture.isKeyDown(_config.toggleKey), dt)) {
        _devConsole->toggle();
    }
    if (_devConsole->isOpen() && wasOpen) {
        readEditing(capture, dt);
    }
    if (_devConsole->isOpen() != wasOpen) {
        // The toggle key also typed a character (`²`, `` ` ``...): it goes neither to the console nor to the game.
        discardPendingInput(capture);
        _caretTimer = 0.0F;
        _caretVisible = true;
    }
    capture.setCaptured(_devConsole->isOpen());

    _caretTimer += dt;
    if (_caretTimer >= _config.caretBlink) {
        _caretTimer = 0.0F;
        _caretVisible = !_caretVisible;
    }
}

void DevConsolePlugin::readEditing(sq::engine::IInputCaptureService& capture, float dt) {
    bool edited = false;
    for (char32_t cp = capture.nextTypedCharacter(); cp != U'\0'; cp = capture.nextTypedCharacter()) {
        _devConsole->type(cp);
        edited = true;
    }
    for (std::size_t i = 0; i < KEY_BINDINGS.size() && _devConsole->isOpen(); ++i) {
        const KeyBinding& binding = KEY_BINDINGS.at(i);
        if (_repeaters.at(i).update(capture.isKeyDown(binding.key), dt)) {
            _devConsole->apply(binding.action);
            edited = true;
        }
    }
    if (const float wheel = capture.mouseWheelDelta(); wheel != 0.0F) {
        _devConsole->scroll(static_cast<int>(std::lround(wheel * WHEEL_LINES)));
    }
    if (edited) {
        _caretTimer = 0.0F;
        _caretVisible = true;
    }
}

void DevConsolePlugin::discardPendingInput(sq::engine::IInputCaptureService& capture) {
    while (capture.nextTypedCharacter() != U'\0') {
    }
    for (std::size_t i = 0; i < KEY_BINDINGS.size(); ++i) {
        _repeaters.at(i).reset(capture.isKeyDown(KEY_BINDINGS.at(i).key));
    }
}

void DevConsolePlugin::lateUpdate(sq::engine::Engine& engine, const sq::time::Time& /*time*/) {
    if (!_devConsole || !_devConsole->isOpen()) {
        return;
    }
    auto window = engine.mediator().getService<sq::engine::IWindowService>();
    auto screen = engine.mediator().getService<sq::engine::IScreenDrawService>();
    if (!window || !screen) {
        return;
    }
    const auto width = static_cast<float>(window->get().width());
    const auto height = static_cast<float>(window->get().height());
    _devConsole->setPageSize(_view.visibleLines(height));
    _view.draw(screen->get(), *_devConsole, width, height, _caretVisible);
}

void DevConsolePlugin::shutdown(sq::engine::Engine& engine) {
    if (auto capture = engine.mediator().getService<sq::engine::IInputCaptureService>(); capture) {
        capture->get().setCaptured(false);
    }
    _devConsole.reset();
}

}  // namespace sq::tools
