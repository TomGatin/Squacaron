#include "input/KeyNames.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "input/KeyboardLayout.hpp"
#include "render/api/InputCodes.hpp"

namespace sq::input {

namespace {

/** @brief Every bindable key, in enum order. */
constexpr std::array KEY_NAMES = {
    KeyName{.key = sq::engine::Key::Apostrophe, .id = "Apostrophe"},
    KeyName{.key = sq::engine::Key::Comma, .id = "Comma"},
    KeyName{.key = sq::engine::Key::Minus, .id = "Minus"},
    KeyName{.key = sq::engine::Key::Period, .id = "Period"},
    KeyName{.key = sq::engine::Key::Slash, .id = "Slash"},
    KeyName{.key = sq::engine::Key::Digit0, .id = "Digit0"},
    KeyName{.key = sq::engine::Key::Digit1, .id = "Digit1"},
    KeyName{.key = sq::engine::Key::Digit2, .id = "Digit2"},
    KeyName{.key = sq::engine::Key::Digit3, .id = "Digit3"},
    KeyName{.key = sq::engine::Key::Digit4, .id = "Digit4"},
    KeyName{.key = sq::engine::Key::Digit5, .id = "Digit5"},
    KeyName{.key = sq::engine::Key::Digit6, .id = "Digit6"},
    KeyName{.key = sq::engine::Key::Digit7, .id = "Digit7"},
    KeyName{.key = sq::engine::Key::Digit8, .id = "Digit8"},
    KeyName{.key = sq::engine::Key::Digit9, .id = "Digit9"},
    KeyName{.key = sq::engine::Key::Semicolon, .id = "Semicolon"},
    KeyName{.key = sq::engine::Key::Equal, .id = "Equal"},
    KeyName{.key = sq::engine::Key::A, .id = "A"},
    KeyName{.key = sq::engine::Key::B, .id = "B"},
    KeyName{.key = sq::engine::Key::C, .id = "C"},
    KeyName{.key = sq::engine::Key::D, .id = "D"},
    KeyName{.key = sq::engine::Key::E, .id = "E"},
    KeyName{.key = sq::engine::Key::F, .id = "F"},
    KeyName{.key = sq::engine::Key::G, .id = "G"},
    KeyName{.key = sq::engine::Key::H, .id = "H"},
    KeyName{.key = sq::engine::Key::I, .id = "I"},
    KeyName{.key = sq::engine::Key::J, .id = "J"},
    KeyName{.key = sq::engine::Key::K, .id = "K"},
    KeyName{.key = sq::engine::Key::L, .id = "L"},
    KeyName{.key = sq::engine::Key::M, .id = "M"},
    KeyName{.key = sq::engine::Key::N, .id = "N"},
    KeyName{.key = sq::engine::Key::O, .id = "O"},
    KeyName{.key = sq::engine::Key::P, .id = "P"},
    KeyName{.key = sq::engine::Key::Q, .id = "Q"},
    KeyName{.key = sq::engine::Key::R, .id = "R"},
    KeyName{.key = sq::engine::Key::S, .id = "S"},
    KeyName{.key = sq::engine::Key::T, .id = "T"},
    KeyName{.key = sq::engine::Key::U, .id = "U"},
    KeyName{.key = sq::engine::Key::V, .id = "V"},
    KeyName{.key = sq::engine::Key::W, .id = "W"},
    KeyName{.key = sq::engine::Key::X, .id = "X"},
    KeyName{.key = sq::engine::Key::Y, .id = "Y"},
    KeyName{.key = sq::engine::Key::Z, .id = "Z"},
    KeyName{.key = sq::engine::Key::LeftBracket, .id = "LeftBracket"},
    KeyName{.key = sq::engine::Key::Backslash, .id = "Backslash"},
    KeyName{.key = sq::engine::Key::RightBracket, .id = "RightBracket"},
    KeyName{.key = sq::engine::Key::Grave, .id = "Grave"},
    KeyName{.key = sq::engine::Key::Space, .id = "Space"},
    KeyName{.key = sq::engine::Key::Escape, .id = "Escape"},
    KeyName{.key = sq::engine::Key::Enter, .id = "Enter"},
    KeyName{.key = sq::engine::Key::Tab, .id = "Tab"},
    KeyName{.key = sq::engine::Key::Backspace, .id = "Backspace"},
    KeyName{.key = sq::engine::Key::Insert, .id = "Insert"},
    KeyName{.key = sq::engine::Key::Delete, .id = "Delete"},
    KeyName{.key = sq::engine::Key::Right, .id = "Right"},
    KeyName{.key = sq::engine::Key::Left, .id = "Left"},
    KeyName{.key = sq::engine::Key::Down, .id = "Down"},
    KeyName{.key = sq::engine::Key::Up, .id = "Up"},
    KeyName{.key = sq::engine::Key::PageUp, .id = "PageUp"},
    KeyName{.key = sq::engine::Key::PageDown, .id = "PageDown"},
    KeyName{.key = sq::engine::Key::Home, .id = "Home"},
    KeyName{.key = sq::engine::Key::End, .id = "End"},
    KeyName{.key = sq::engine::Key::CapsLock, .id = "CapsLock"},
    KeyName{.key = sq::engine::Key::ScrollLock, .id = "ScrollLock"},
    KeyName{.key = sq::engine::Key::NumLock, .id = "NumLock"},
    KeyName{.key = sq::engine::Key::PrintScreen, .id = "PrintScreen"},
    KeyName{.key = sq::engine::Key::Pause, .id = "Pause"},
    KeyName{.key = sq::engine::Key::F1, .id = "F1"},
    KeyName{.key = sq::engine::Key::F2, .id = "F2"},
    KeyName{.key = sq::engine::Key::F3, .id = "F3"},
    KeyName{.key = sq::engine::Key::F4, .id = "F4"},
    KeyName{.key = sq::engine::Key::F5, .id = "F5"},
    KeyName{.key = sq::engine::Key::F6, .id = "F6"},
    KeyName{.key = sq::engine::Key::F7, .id = "F7"},
    KeyName{.key = sq::engine::Key::F8, .id = "F8"},
    KeyName{.key = sq::engine::Key::F9, .id = "F9"},
    KeyName{.key = sq::engine::Key::F10, .id = "F10"},
    KeyName{.key = sq::engine::Key::F11, .id = "F11"},
    KeyName{.key = sq::engine::Key::F12, .id = "F12"},
    KeyName{.key = sq::engine::Key::LeftShift, .id = "LeftShift"},
    KeyName{.key = sq::engine::Key::LeftControl, .id = "LeftControl"},
    KeyName{.key = sq::engine::Key::LeftAlt, .id = "LeftAlt"},
    KeyName{.key = sq::engine::Key::LeftSuper, .id = "LeftSuper"},
    KeyName{.key = sq::engine::Key::RightShift, .id = "RightShift"},
    KeyName{.key = sq::engine::Key::RightControl, .id = "RightControl"},
    KeyName{.key = sq::engine::Key::RightAlt, .id = "RightAlt"},
    KeyName{.key = sq::engine::Key::RightSuper, .id = "RightSuper"},
    KeyName{.key = sq::engine::Key::KeyboardMenu, .id = "KeyboardMenu"},
    KeyName{.key = sq::engine::Key::Keypad0, .id = "Keypad0"},
    KeyName{.key = sq::engine::Key::Keypad1, .id = "Keypad1"},
    KeyName{.key = sq::engine::Key::Keypad2, .id = "Keypad2"},
    KeyName{.key = sq::engine::Key::Keypad3, .id = "Keypad3"},
    KeyName{.key = sq::engine::Key::Keypad4, .id = "Keypad4"},
    KeyName{.key = sq::engine::Key::Keypad5, .id = "Keypad5"},
    KeyName{.key = sq::engine::Key::Keypad6, .id = "Keypad6"},
    KeyName{.key = sq::engine::Key::Keypad7, .id = "Keypad7"},
    KeyName{.key = sq::engine::Key::Keypad8, .id = "Keypad8"},
    KeyName{.key = sq::engine::Key::Keypad9, .id = "Keypad9"},
    KeyName{.key = sq::engine::Key::KeypadDecimal, .id = "KeypadDecimal"},
    KeyName{.key = sq::engine::Key::KeypadDivide, .id = "KeypadDivide"},
    KeyName{.key = sq::engine::Key::KeypadMultiply, .id = "KeypadMultiply"},
    KeyName{.key = sq::engine::Key::KeypadSubtract, .id = "KeypadSubtract"},
    KeyName{.key = sq::engine::Key::KeypadAdd, .id = "KeypadAdd"},
    KeyName{.key = sq::engine::Key::KeypadEnter, .id = "KeypadEnter"},
    KeyName{.key = sq::engine::Key::KeypadEqual, .id = "KeypadEqual"},
};

/**
 * @brief Gets the key printed at the position of a QWERTY key on an AZERTY keyboard.
 * @param key Key, named after its QWERTY position.
 * @return The key whose QWERTY name is the AZERTY label, or `key` when both layouts agree.
 */
sq::engine::Key azertyLabelKey(sq::engine::Key key) noexcept {
    switch (key) {
        case sq::engine::Key::A:
            return sq::engine::Key::Q;
        case sq::engine::Key::Q:
            return sq::engine::Key::A;
        case sq::engine::Key::W:
            return sq::engine::Key::Z;
        case sq::engine::Key::Z:
            return sq::engine::Key::W;
        case sq::engine::Key::Semicolon:
            return sq::engine::Key::M;
        case sq::engine::Key::M:
            return sq::engine::Key::Comma;
        default:
            return key;
    }
}

}  // namespace

std::span<const KeyName> keyNames() noexcept { return KEY_NAMES; }

std::string_view keyId(sq::engine::Key key) noexcept {
    // NOLINTNEXTLINE(readability-qualified-auto): std::array iterator is not a pointer on MSVC.
    const auto found = std::ranges::find(KEY_NAMES, key, &KeyName::key);
    return found != KEY_NAMES.end() ? found->id : std::string_view{};
}

std::optional<sq::engine::Key> keyFromId(std::string_view id) noexcept {
    // NOLINTNEXTLINE(readability-qualified-auto): std::array iterator is not a pointer on MSVC.
    const auto found = std::ranges::find(KEY_NAMES, id, &KeyName::id);
    if (found == KEY_NAMES.end()) {
        return std::nullopt;
    }
    return found->key;
}

std::string keyLabel(sq::engine::Key key, KeyboardLayout layout) {
    if (layout == KeyboardLayout::Azerty) {
        key = azertyLabelKey(key);
    }
    const std::string_view id = keyId(key);
    if (id.empty()) {
        return "NONE";
    }
    // "LeftShift" -> "LEFT SHIFT": a space before each upper-case letter that follows a lower-case one.
    std::string label;
    for (std::size_t i = 0; i < id.size(); ++i) {
        const auto c = static_cast<unsigned char>(id[i]);
        if (i > 0 && std::isupper(c) != 0 && std::islower(static_cast<unsigned char>(id[i - 1])) != 0) {
            label.push_back(' ');
        }
        label.push_back(static_cast<char>(std::toupper(c)));
    }
    return label;
}

}  // namespace sq::input
