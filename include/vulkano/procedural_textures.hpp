#pragma once

#include <cstdint>
#include <vector>

namespace vulkano {

[[nodiscard]] auto generate_checkerboard_rgba_srgb(
    std::uint32_t resolution,
    std::uint32_t tilesPerSide) -> std::vector<std::uint8_t>;

[[nodiscard]] auto generate_normal_map_rgba(
    std::uint32_t resolution,
    std::uint32_t seed,
    float amplitude) -> std::vector<std::uint8_t>;

} // namespace vulkano

