/**
 * @file InputPlugin.cpp
 * @brief Implementation of InputPlugin and its abstract service adapter.
 */

#include "input/InputPlugin.hpp"

#include <memory>
#include <utility>

#include "core/Engine.hpp"
#include "core/Mediator.hpp"
#include "core/MediatorError.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "input/IInputService.hpp"
#include "render/api/IWindowService.hpp"
#include "systems/input/InputSystem.hpp"

namespace sq::input {

/**
 * @brief Concrete adapter implementing IInputService via InputSystem.
 */
class InputPlugin::InputServiceImpl final : public sq::engine::IInputService {
  public:
    /**
     * @brief Constructs InputServiceImpl wrapping an InputSystem reference.
     * @param system Reference to the active InputSystem.
     */
    explicit InputServiceImpl(sq::systems::InputSystem& system) : _system(&system) {}

    [[nodiscard]] ActionMapping& mapping() noexcept override { return _system->mapping(); }

    [[nodiscard]] const ActionMapping& mapping() const noexcept override { return _system->mapping(); }

    [[nodiscard]] int playerIndex() const noexcept override { return _system->playerIndex(); }

    [[nodiscard]] int gamepadIndex() const noexcept override { return _system->gamepadIndex(); }

  private:
    /** @brief Pointer to wrapped InputSystem. */
    sq::systems::InputSystem* _system;
};

InputPlugin::InputPlugin(ActionMapping mapping, int playerIndex, int gamepadIndex)
    : _inputSystem(std::move(mapping), playerIndex, gamepadIndex) {}

InputPlugin::~InputPlugin() = default;

void InputPlugin::build(sq::engine::PluginBuilder& builder) {
    builder.require<sq::engine::CorePlugin>();
    builder.requireService<sq::engine::IWindowService>();
    builder.provides<sq::engine::IInputService>();
}

void InputPlugin::init(sq::engine::Engine& engine) {
    _inputService = std::make_shared<InputServiceImpl>(_inputSystem);
    if (const auto res = engine.mediator().registerService<sq::engine::IInputService>(_inputService); !res) {
        throw sq::engine::MediatorError(res.error());
    }
}

void InputPlugin::preUpdate(sq::engine::Engine& engine) {
    _inputSystem.update(engine.service<sq::engine::IWindowService>(), engine.registry(), &engine.eventBus());
}

void InputPlugin::shutdown(sq::engine::Engine& engine) {
    if (engine.mediator().hasService<sq::engine::IInputService>()) {
        engine.mediator().removeService<sq::engine::IInputService>();
    }
    _inputService.reset();
}

}  // namespace sq::input
