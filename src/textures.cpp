#include <vulkano/textures.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace vulkano {

namespace {
    // Clamp float [0,1] to byte [0,255]
    [[nodiscard]] constexpr std::uint8_t to_byte(float v) noexcept {
        const float clamped {v < 0.0F ? 0.0F : (v > 1.0F ? 1.0F : v)};
        return static_cast<std::uint8_t>(clamped * 255.0F + 0.5F);
    }

    // Simple Wang hash for 32-bit values
    [[nodiscard]] constexpr std::uint32_t wang_hash(std::uint32_t x) noexcept {
        x = (x ^ 61U) ^ (x >> 16U);
        x *= 9U;
        x = x ^ (x >> 4U);
        x *= 0x27d4eb2dU;
        x = x ^ (x >> 15U);
        return x;
    }

    // Pseudo-random in [0,1]
    [[nodiscard]] constexpr float rand01(std::uint32_t seed) noexcept {
        return static_cast<float>(wang_hash(seed)) / static_cast<float>(std::numeric_limits<std::uint32_t>::max());
    }
}

ImageRGBA8 make_checkerboard_rgba(
    std::uint32_t size,
    std::uint32_t squares,
    const glm::vec3& colorLight,
    const glm::vec3& colorDark) noexcept {
    ImageRGBA8 img {};
    if (size == 0U || squares == 0U) {
        return img;
    }
    const std::uint32_t width {size};
    const std::uint32_t height {size};
    const std::uint32_t channels {4U};
    const std::uint32_t pixels {width * height};
    img.width = width;
    img.height = height;
    img.pixels.assign(static_cast<std::size_t>(pixels * channels), std::uint8_t {0U});

    const std::uint32_t squareSize {squares == 0U ? 1U : (size / squares)};
    const glm::vec3 light {colorLight};
    const glm::vec3 dark {colorDark};
    for (std::uint32_t y {0U}; y < height; ++y) {
        for (std::uint32_t x {0U}; x < width; ++x) {
            const std::uint32_t cx {(x / squareSize)};
            const std::uint32_t cy {(y / squareSize)};
            const bool isLight {((cx + cy) & 1U) == 0U};
            const glm::vec3 c {isLight ? light : dark};
            const std::size_t idx {static_cast<std::size_t>((y * width + x) * channels)};
            img.pixels[idx + 0U] = to_byte(c.x);
            img.pixels[idx + 1U] = to_byte(c.y);
            img.pixels[idx + 2U] = to_byte(c.z);
            img.pixels[idx + 3U] = 255U;
        }
    }
    return img;
}

ImageRGBA8 make_blue_noise_normal_rgba(
    std::uint32_t size,
    float amplitude) noexcept {
    ImageRGBA8 img {};
    if (size == 0U) {
        return img;
    }
    const std::uint32_t width {size};
    const std::uint32_t height {size};
    const std::uint32_t channels {4U};
    const std::uint32_t pixels {width * height};
    img.width = width;
    img.height = height;
    img.pixels.assign(static_cast<std::size_t>(pixels * channels), std::uint8_t {0U});

    // Step 1: generate two high-frequency random fields in [0,1]
    std::vector<float> nxF(static_cast<std::size_t>(pixels), 0.0F);
    std::vector<float> nyF(static_cast<std::size_t>(pixels), 0.0F);

    // Use two distinct seeds for low correlation; ensure wrap via modulo arithmetic only
    const std::uint32_t seedX {0x1234ABCDU};
    const std::uint32_t seedY {0xDEADBEEFU};
    for (std::uint32_t y {0U}; y < height; ++y) {
        for (std::uint32_t x {0U}; x < width; ++x) {
            const std::size_t idx {static_cast<std::size_t>(y * width + x)};
            const std::uint32_t keyX {(x * 73856093U) ^ (y * 19349663U) ^ seedX};
            const std::uint32_t keyY {(x * 83492791U) ^ (y * 2971215073U) ^ seedY};
            nxF[idx] = rand01(keyX);
            nyF[idx] = rand01(keyY);
        }
    }

    // Step 2: high-pass via subtracting a 3x3 box blur (with wrap addressing)
    const std::array<int, 9> offsetsX { -1, 0, 1, -1, 0, 1, -1, 0, 1 };
    const std::array<int, 9> offsetsY { -1, -1, -1, 0, 0, 0, 1, 1, 1 };
    std::vector<float> nxH(nxF);
    std::vector<float> nyH(nyF);
    for (std::uint32_t y {0U}; y < height; ++y) {
        for (std::uint32_t x {0U}; x < width; ++x) {
            float sumX {0.0F};
            float sumY {0.0F};
            for (std::size_t k {0U}; k < offsetsX.size(); ++k) {
                const std::uint32_t sx {static_cast<std::uint32_t>((static_cast<int>(x) + offsetsX[k] + static_cast<int>(width)) % static_cast<int>(width))};
                const std::uint32_t sy {static_cast<std::uint32_t>((static_cast<int>(y) + offsetsY[k] + static_cast<int>(height)) % static_cast<int>(height))};
                const std::size_t sidx {static_cast<std::size_t>(sy * width + sx)};
                sumX += nxF[sidx];
                sumY += nyF[sidx];
            }
            const float avgX {sumX / 9.0F};
            const float avgY {sumY / 9.0F};
            const std::size_t idx {static_cast<std::size_t>(y * width + x)};
            nxH[idx] = nxF[idx] - avgX;
            nyH[idx] = nyF[idx] - avgY;
        }
    }

    // Step 3: map to [-1,1] cone and compute nz
    const float amp {amplitude};
    for (std::uint32_t y {0U}; y < height; ++y) {
        for (std::uint32_t x {0U}; x < width; ++x) {
            const std::size_t idx1D {static_cast<std::size_t>(y * width + x)};
            // Center to [-0.5, 0.5] then scale to [-1,1] with amplitude
            float nx {nxH[idx1D]};
            float ny {nyH[idx1D]};
            // Normalize high-pass range roughly to [-0.5,0.5] via a conservative scale
            const float scaleHP {1.0F};
            nx = (nx * scaleHP) * 2.0F;
            ny = (ny * scaleHP) * 2.0F;
            nx *= amp;
            ny *= amp;
            const float nzSq {1.0F - nx * nx - ny * ny};
            const float nz {nzSq > 0.0F ? std::sqrt(nzSq) : 0.0F};
            const float r {0.5F * (nx + 1.0F)};
            const float g {0.5F * (ny + 1.0F)};
            const float b {0.5F * (nz + 1.0F)};
            const std::size_t idx {static_cast<std::size_t>(idx1D * channels)};
            img.pixels[idx + 0U] = to_byte(r);
            img.pixels[idx + 1U] = to_byte(g);
            img.pixels[idx + 2U] = to_byte(b);
            img.pixels[idx + 3U] = 255U;
        }
    }

    return img;
}

} // namespace vulkano

