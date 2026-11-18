#pragma once

#include "render/api/IScreenDrawService.hpp"

namespace sq::ui {

/** @brief Handle to a font loaded with `sq::engine::IScreenDrawService::loadFont`. */
using sq::engine::FontId;

/** @brief Sentinel value meaning "no font / use the default font". */
using sq::engine::NO_FONT;

}  // namespace sq::ui
