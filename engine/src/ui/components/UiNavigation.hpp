/**
 * @file UiNavigation.hpp
 * @brief Resources configuring focus navigation and holding the focused widget.
 */

#pragma once

#include <vector>

#include "ecs/Entity.hpp"
#include "render/api/InputCodes.hpp"
#include "render/api/components/Renderable.hpp"

namespace sq::ui {

/** @brief Keys and gamepad buttons bound to one navigation command. */
struct UiNavBinding {
    /** @brief Keyboard keys. */
    std::vector<sq::engine::Key> keys;
    /** @brief Gamepad buttons. */
    std::vector<sq::engine::GamepadButton> buttons;
};

/**
 * @brief Registry resource configuring focus navigation; emplaced with these defaults by `UiPlugin`.
 *
 * Games rebind it freely (`registry.getResource<sq::ui::UiNavigation>()`). Space confirms, except while a text input
 * has the focus, where it types a space.
 */
struct UiNavigation {
    /** @brief Previous widget. */
    UiNavBinding up{.keys = {sq::engine::Key::Up}, .buttons = {sq::engine::GamepadButton::LeftFaceUp}};
    /** @brief Next widget. */
    UiNavBinding down{.keys = {sq::engine::Key::Down, sq::engine::Key::Tab},
                      .buttons = {sq::engine::GamepadButton::LeftFaceDown}};
    /** @brief Decrease a slider, else previous widget. */
    UiNavBinding left{.keys = {sq::engine::Key::Left}, .buttons = {sq::engine::GamepadButton::LeftFaceLeft}};
    /** @brief Increase a slider, else next widget. */
    UiNavBinding right{.keys = {sq::engine::Key::Right}, .buttons = {sq::engine::GamepadButton::LeftFaceRight}};
    /** @brief Activate the focused widget. */
    UiNavBinding confirm{
        .keys = {sq::engine::Key::Enter, sq::engine::Key::Space},
        .buttons = {sq::engine::GamepadButton::RightFaceDown},
    };
    /** @brief Publish `UiBackEvent`. */
    UiNavBinding back{.keys = {sq::engine::Key::Escape}, .buttons = {sq::engine::GamepadButton::RightFaceRight}};
    /** @brief Gamepad read for navigation. */
    int gamepad = 0;
    /** @brief Colour of the outline drawn around the focused widget. */
    sq::ecs::Rgba focusColor = {.r = 255, .g = 210, .b = 60, .a = 255};
    /** @brief Thickness of that outline in pixels. */
    float focusThickness = 2.0F;
};

/** @brief Registry resource holding the focused widget; emplaced by `UiPlugin`. */
struct UiFocus {
    /** @brief Focused widget, or `sq::ecs::NULL_ENTITY`. Set it to move the focus from game code. */
    sq::ecs::Entity focused = sq::ecs::NULL_ENTITY;
    /** @brief Blink phase of the caret of a focused text input; driven by the UI interaction system. */
    bool caretVisible = true;
};

}  // namespace sq::ui
