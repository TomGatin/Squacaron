#pragma once

#include <cstddef>
#include <numbers>

namespace render::detail {

inline constexpr std::size_t INITIAL_QUEUE_CAPACITY = 1024;
inline constexpr float PLACEHOLDER_SIZE = 32.0F;
inline constexpr float RADIANS_TO_DEGREES = 180.0F / std::numbers::pi_v<float>;

}  // namespace render::detail
