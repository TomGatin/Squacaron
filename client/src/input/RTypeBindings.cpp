#include "input/RTypeBindings.hpp"

#include <algorithm>
#include <cstddef>
#include <string_view>

#include "input/ActionId.hpp"
#include "input/ActionMapping.hpp"
#include "render/api/InputCodes.hpp"

namespace client::input {

namespace {

/**
 * @brief Gets the slot of a control in `ControlKeys`.
 * @param control Control.
 * @return Its index.
 */
constexpr std::size_t slot(Control control) noexcept { return static_cast<std::size_t>(control); }

/**
 * @brief Gets the secondary key of a control.
 * @param control Control.
 * @return The key, or `sq::engine::Key::Unknown` if it has none.
 */
constexpr sq::engine::Key secondaryKey(Control control) noexcept {
    switch (control) {
        case Control::MoveUp:
            return sq::engine::Key::Up;
        case Control::MoveDown:
            return sq::engine::Key::Down;
        case Control::MoveLeft:
            return sq::engine::Key::Left;
        case Control::MoveRight:
            return sq::engine::Key::Right;
        case Control::Shoot:
            return sq::engine::Key::Enter;
        case Control::Charge:
            return sq::engine::Key::RightShift;
        case Control::Ready:
            return sq::engine::Key::Unknown;
    }
    return sq::engine::Key::Unknown;
}

/**
 * @brief Gets the secondary key of a control, unless a main key already uses it.
 * @param keys Main keys.
 * @param control Control.
 * @return The key, or `sq::engine::Key::Unknown`.
 */
sq::engine::Key freeSecondaryKey(const ControlKeys& keys, Control control) noexcept {
    const sq::engine::Key key = secondaryKey(control);
    return std::ranges::contains(keys, key) ? sq::engine::Key::Unknown : key;
}

/**
 * @brief Binds a button action to its main key, its secondary key and a gamepad button.
 * @param mapping Mapping to fill.
 * @param keys Main keys.
 * @param control Control of the action.
 * @param action Action name.
 * @param pad Gamepad button.
 */
void bindButton(sq::input::ActionMapping& mapping, const ControlKeys& keys, Control control, std::string_view action,
                sq::engine::GamepadButton pad) {
    mapping.registerAction(action, sq::input::ActionType::Button);
    mapping.bindKey(action, keys.at(slot(control)));
    if (const sq::engine::Key extra = freeSecondaryKey(keys, control); extra != sq::engine::Key::Unknown) {
        mapping.bindKey(action, extra);
    }
    mapping.bindGamepadButton(action, pad);
}

/**
 * @brief Binds an axis action to two pairs of keys (main, then secondary) and a gamepad stick axis.
 * @param mapping Mapping to fill.
 * @param keys Main keys.
 * @param negative Control of the negative side.
 * @param positive Control of the positive side.
 * @param action Action name.
 * @param axis Gamepad axis.
 */
void bindAxis(sq::input::ActionMapping& mapping, const ControlKeys& keys, Control negative, Control positive,
              std::string_view action, sq::engine::GamepadAxis axis) {
    mapping.registerAction(action, sq::input::ActionType::Axis);
    mapping.bindAxisComposite(action, keys.at(slot(negative)), keys.at(slot(positive)));
    mapping.bindAxisComposite(action, freeSecondaryKey(keys, negative), freeSecondaryKey(keys, positive));
    mapping.bindGamepadAxis(action, axis, 1.0F);
}

}  // namespace

std::string_view controlId(Control control) noexcept {
    switch (control) {
        case Control::MoveUp:
            return "move_up";
        case Control::MoveDown:
            return "move_down";
        case Control::MoveLeft:
            return "move_left";
        case Control::MoveRight:
            return "move_right";
        case Control::Shoot:
            return "shoot";
        case Control::Charge:
            return "charge";
        case Control::Ready:
            return "ready";
    }
    return "";
}

std::string_view controlLabel(Control control) noexcept {
    switch (control) {
        case Control::MoveUp:
            return "MOVE UP";
        case Control::MoveDown:
            return "MOVE DOWN";
        case Control::MoveLeft:
            return "MOVE LEFT";
        case Control::MoveRight:
            return "MOVE RIGHT";
        case Control::Shoot:
            return "SHOOT";
        case Control::Charge:
            return "CHARGE";
        case Control::Ready:
            return "READY";
    }
    return "";
}

ControlKeys defaultControlKeys() noexcept {
    ControlKeys keys{};
    keys.at(slot(Control::MoveUp)) = sq::engine::Key::W;
    keys.at(slot(Control::MoveDown)) = sq::engine::Key::S;
    keys.at(slot(Control::MoveLeft)) = sq::engine::Key::A;
    keys.at(slot(Control::MoveRight)) = sq::engine::Key::D;
    keys.at(slot(Control::Shoot)) = sq::engine::Key::Space;
    keys.at(slot(Control::Charge)) = sq::engine::Key::LeftShift;
    keys.at(slot(Control::Ready)) = sq::engine::Key::R;
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

void setupBindings(sq::input::ActionMapping& mapping, const ControlKeys& keys) {
    mapping.clear();
    bindAxis(mapping, keys, Control::MoveLeft, Control::MoveRight, "move_x", sq::engine::GamepadAxis::LeftX);
    bindAxis(mapping, keys, Control::MoveUp, Control::MoveDown, "move_y", sq::engine::GamepadAxis::LeftY);
    bindButton(mapping, keys, Control::Shoot, "shoot", sq::engine::GamepadButton::RightFaceDown);
    bindButton(mapping, keys, Control::Charge, "charge_weapon", sq::engine::GamepadButton::RightTrigger2);
    bindButton(mapping, keys, Control::Ready, "ready", sq::engine::GamepadButton::MiddleRight);
}

void setupDefaultBindings(sq::input::ActionMapping& mapping) { setupBindings(mapping, defaultControlKeys()); }

}  // namespace client::input
