#include "Controls.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

#include "config/Settings.hpp"
#include "game/Components.hpp"
#include "input/ActionId.hpp"
#include "input/ActionMapping.hpp"
#include "input/KeyNames.hpp"
#include "render/api/InputCodes.hpp"

namespace rlss::client {

namespace {

/**
 * @brief Gets the slot of a control in `ControlKeys`.
 * @param control Control.
 * @return Its index.
 */
constexpr std::size_t slot(Control control) noexcept { return static_cast<std::size_t>(control); }

/**
 * @brief Gets the second key of a control.
 * @param control Control.
 * @return The key, or `sq::engine::Key::Unknown` if it has none.
 */
constexpr sq::engine::Key secondaryKey(Control control) noexcept {
    switch (control) {
        case Control::SteerLeft:
            return sq::engine::Key::Left;
        case Control::SteerRight:
            return sq::engine::Key::Right;
        case Control::NoseUp:
            return sq::engine::Key::Up;
        case Control::NoseDown:
            return sq::engine::Key::Down;
        case Control::Jump:
            return sq::engine::Key::Unknown;
        case Control::Boost:
            return sq::engine::Key::RightShift;
    }
    return sq::engine::Key::Unknown;
}

/**
 * @brief Gets the second key of a control, unless a main key already uses it.
 * @param keys Main keys.
 * @param control Control.
 * @return The key, or `sq::engine::Key::Unknown`.
 */
sq::engine::Key freeSecondaryKey(const ControlKeys& keys, Control control) noexcept {
    const sq::engine::Key key = secondaryKey(control);
    return std::ranges::contains(keys, key) ? sq::engine::Key::Unknown : key;
}

/**
 * @brief Binds a button action to the main key of a control and to its free second key.
 * @param mapping Mapping to fill.
 * @param keys Main keys.
 * @param control Control of the action.
 * @param action Action name.
 */
void bindButtonKeys(sq::input::ActionMapping& mapping, const ControlKeys& keys, Control control,
                    std::string_view action) {
    mapping.registerAction(action, sq::input::ActionType::Button);
    mapping.bindKey(action, keys.at(slot(control)));
    if (const sq::engine::Key extra = freeSecondaryKey(keys, control); extra != sq::engine::Key::Unknown) {
        mapping.bindKey(action, extra);
    }
}

/**
 * @brief Binds an axis action to the main keys of two controls, then to their free second keys.
 * @param mapping Mapping to fill.
 * @param keys Main keys.
 * @param negative Control of the negative side.
 * @param positive Control of the positive side.
 * @param action Action name.
 */
void bindAxisKeys(sq::input::ActionMapping& mapping, const ControlKeys& keys, Control negative, Control positive,
                  std::string_view action) {
    mapping.registerAction(action, sq::input::ActionType::Axis);
    mapping.bindAxisComposite(action, keys.at(slot(negative)), keys.at(slot(positive)));
    mapping.bindAxisComposite(action, freeSecondaryKey(keys, negative), freeSecondaryKey(keys, positive));
}

}  // namespace

std::string_view controlId(Control control) noexcept {
    switch (control) {
        case Control::SteerLeft:
            return "steer_left";
        case Control::SteerRight:
            return "steer_right";
        case Control::NoseUp:
            return "nose_up";
        case Control::NoseDown:
            return "nose_down";
        case Control::Jump:
            return "jump";
        case Control::Boost:
            return "boost";
    }
    return "";
}

std::string_view controlLabel(Control control) noexcept {
    switch (control) {
        case Control::SteerLeft:
            return "GAUCHE";
        case Control::SteerRight:
            return "DROITE";
        case Control::NoseUp:
            return "NEZ EN HAUT";
        case Control::NoseDown:
            return "NEZ EN BAS";
        case Control::Jump:
            return "SAUT";
        case Control::Boost:
            return "BOOST";
    }
    return "";
}

std::string controlSetting(Control control) { return std::string(CONTROLS_PREFIX).append(controlId(control)); }

ControlKeys defaultControlKeys() noexcept {
    ControlKeys keys{};
    keys.at(slot(Control::SteerLeft)) = sq::engine::Key::A;
    keys.at(slot(Control::SteerRight)) = sq::engine::Key::D;
    keys.at(slot(Control::NoseUp)) = sq::engine::Key::W;
    keys.at(slot(Control::NoseDown)) = sq::engine::Key::S;
    keys.at(slot(Control::Jump)) = sq::engine::Key::Space;
    keys.at(slot(Control::Boost)) = sq::engine::Key::LeftShift;
    return keys;
}

void assignKey(ControlKeys& keys, Control control, sq::engine::Key key) noexcept {
    sq::engine::Key& target = keys.at(slot(control));
    // NOLINTNEXTLINE(readability-qualified-auto): std::array iterator is not a pointer on MSVC.
    if (auto other = std::ranges::find(keys, key); other != keys.end()) {
        *other = target;
    }
    target = key;
}

void defineControlSettings(sq::config::Settings& settings) {
    const ControlKeys defaults = defaultControlKeys();
    for (const Control control : CONTROLS) {
        settings.define(controlSetting(control), std::string(sq::input::keyId(defaults.at(slot(control)))));
    }
}

ControlKeys readControlKeys(const sq::config::Settings& settings) {
    ControlKeys keys = defaultControlKeys();
    for (const Control control : CONTROLS) {
        const std::string key = controlSetting(control);
        if (!settings.has(key)) {
            continue;
        }
        if (const auto saved = sq::input::keyFromId(settings.getString(key))) {
            keys.at(slot(control)) = *saved;
        }
    }
    return keys;
}

void writeControlKeys(sq::config::Settings& settings, const ControlKeys& keys) {
    for (const Control control : CONTROLS) {
        (void)settings.set(controlSetting(control), std::string(sq::input::keyId(keys.at(slot(control)))));
    }
}

void setupBindings(sq::input::ActionMapping& mapping, const ControlKeys& keys) {
    mapping.clear();
    bindAxisKeys(mapping, keys, Control::SteerLeft, Control::SteerRight, game::ACTION_STEER);
    mapping.bindGamepadAxis(game::ACTION_STEER, sq::engine::GamepadAxis::LeftX, 1.0F);

    bindAxisKeys(mapping, keys, Control::NoseDown, Control::NoseUp, game::ACTION_PITCH);
    mapping.bindGamepadAxis(game::ACTION_PITCH, sq::engine::GamepadAxis::LeftY, -1.0F);  // the stick is negative up

    bindButtonKeys(mapping, keys, Control::Jump, game::ACTION_JUMP);
    mapping.bindGamepadButton(game::ACTION_JUMP, sq::engine::GamepadButton::RightFaceDown);

    bindButtonKeys(mapping, keys, Control::Boost, game::ACTION_BOOST);
    mapping.bindGamepadButton(game::ACTION_BOOST, sq::engine::GamepadButton::RightFaceRight);
    mapping.bindGamepadButton(game::ACTION_BOOST, sq::engine::GamepadButton::RightTrigger2);
}

sq::input::ActionMapping makeControls() {
    sq::input::ActionMapping mapping;
    setupBindings(mapping, defaultControlKeys());
    return mapping;
}

}  // namespace rlss::client
