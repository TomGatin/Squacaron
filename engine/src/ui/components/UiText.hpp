#pragma once

#include <string>

#include "render/api/components/Renderable.hpp"
#include "ui/components/FontId.hpp"

namespace sq::ui {

/**
 * @brief Draws a text string in screen space.
 *
 * Requires a `UiAnchor` on the same entity to determine screen position.
 * `fontId` is resolved by `FontStore`; `NO_FONT` falls back to the raylib default font.
 */
struct UiText {
    /** @brief Text content to display. */
    std::string text;

    /** @brief Font size in pixels. */
    float fontSize = 20.0F;

    /** @brief Text color. */
    sq::ecs::Rgba color;

    /** @brief Font to use, or `NO_FONT` for the default font. */
    FontId fontId = NO_FONT;
};

}  // namespace sq::ui
