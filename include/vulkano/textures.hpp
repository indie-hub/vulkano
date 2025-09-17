// textures.hpp
#pragma once

#include <cstdint>
#include <vector>

#include <glm/vec3.hpp>

namespace vulkano {

// RGBA8 image container (row-major, 4 bytes per pixel)
struct ImageRGBA8 final {
    std::uint32_t width {0U};
    std::uint32_t height {0U};
    std::vector<std::uint8_t> pixels {};
};

// Procedural checkerboard albedo (sRGB-like RGBA8). Size must be power-of-two for predictable tiling.
// - size: texture width and height in texels
// - squares: number of squares per row/column (must divide size)
// - colorLight/colorDark: sRGB colors in [0,1] triplets; A set to 1.0
[[nodiscard]] ImageRGBA8 make_checkerboard_rgba(
    std::uint32_t size,
    std::uint32_t squares,
    const glm::vec3& colorLight,
    const glm::vec3& colorDark) noexcept;

// Procedural blue-noise-ish normal map (tangent-space), packed RGBA8 (A=255).
// - size: texture width and height in texels
// - amplitude: scales nx, ny in [-1,1] cone before recomputing nz
[[nodiscard]] ImageRGBA8 make_blue_noise_normal_rgba(
    std::uint32_t size,
    float amplitude) noexcept;

} // namespace vulkano

