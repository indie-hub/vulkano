#include <vulkano/procedural_textures.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

namespace {
    [[nodiscard]] auto validate_resolution(std::uint32_t resolution) -> std::uint32_t {
        if(resolution == 0U) {
            throw std::invalid_argument {"Texture resolution must be greater than zero"};
        }
        return resolution;
    }

    [[nodiscard]] auto validate_tiles(std::uint32_t tiles) -> std::uint32_t {
        if(tiles == 0U) {
            throw std::invalid_argument {"Checkerboard tiles per side must be greater than zero"};
        }
        return tiles;
    }

    [[nodiscard]] auto hash_coords(std::int32_t x, std::int32_t y, std::uint32_t seed) -> std::uint32_t {
        constexpr std::uint32_t prime1 {0x9E3779B1U};
        constexpr std::uint32_t prime2 {0x85EBCA77U};
        std::uint32_t value = static_cast<std::uint32_t>(x);
        value ^= prime1;
        value = (value << 13) | (value >> 19);
        value += static_cast<std::uint32_t>(y) * prime2;
        value ^= seed * 0xC2B2AE3DU;
        value ^= value >> 16U;
        value *= 0x7FEB352DU;
        value ^= value >> 15U;
        value *= 0x846CA68BU;
        value ^= value >> 16U;
        return value;
    }

    [[nodiscard]] auto smooth_step(float value) -> float {
        return value * value * (3.0F - (2.0F * value));
    }

    [[nodiscard]] auto value_noise(float x, float y, std::uint32_t seed) -> float {
        const std::int32_t x0 = static_cast<std::int32_t>(std::floor(x));
        const std::int32_t y0 = static_cast<std::int32_t>(std::floor(y));
        const std::int32_t x1 = x0 + 1;
        const std::int32_t y1 = y0 + 1;

        const float sx = smooth_step(x - static_cast<float>(x0));
        const float sy = smooth_step(y - static_cast<float>(y0));

        const float n00 = static_cast<float>(hash_coords(x0, y0, seed) & 0xFFFFU) / 65535.0F;
        const float n10 = static_cast<float>(hash_coords(x1, y0, seed) & 0xFFFFU) / 65535.0F;
        const float n01 = static_cast<float>(hash_coords(x0, y1, seed) & 0xFFFFU) / 65535.0F;
        const float n11 = static_cast<float>(hash_coords(x1, y1, seed) & 0xFFFFU) / 65535.0F;

        const float ix0 = glm::mix(n00, n10, sx);
        const float ix1 = glm::mix(n01, n11, sx);
        return glm::mix(ix0, ix1, sy);
    }

    [[nodiscard]] auto srgb_encode(float value) -> std::uint8_t {
        const float encoded = glm::clamp(value, 0.0F, 1.0F);
        return static_cast<std::uint8_t>(std::lround(encoded * 255.0F));
    }

    [[nodiscard]] auto pack_normal(const glm::vec3& normal) -> std::array<std::uint8_t, 4U> {
        const glm::vec3 encoded = (glm::clamp(normal, glm::vec3 {-1.0F}, glm::vec3 {1.0F}) * 0.5F) + 0.5F;
        return {
            srgb_encode(encoded.x),
            srgb_encode(encoded.y),
            srgb_encode(encoded.z),
            255U
        };
    }
}

namespace vulkano {

auto generate_checkerboard_rgba_srgb(std::uint32_t resolution, std::uint32_t tilesPerSide) -> std::vector<std::uint8_t> {
    resolution = validate_resolution(resolution);
    tilesPerSide = validate_tiles(tilesPerSide);

    const std::uint32_t tileResolution = std::max(resolution / tilesPerSide, 1U);
    const std::size_t pixelCount = static_cast<std::size_t>(resolution) * static_cast<std::size_t>(resolution);
    std::vector<std::uint8_t> pixels(pixelCount * 4U, 255U);

    constexpr std::array<float, 3U> lightColour {0.8F, 0.8F, 0.8F};
    constexpr std::array<float, 3U> darkColour {0.2F, 0.2F, 0.2F};

    for(std::uint32_t y {0U}; y < resolution; ++y) {
        const std::uint32_t tileY = (y / tileResolution) % tilesPerSide;
        for(std::uint32_t x {0U}; x < resolution; ++x) {
            const std::uint32_t tileX = (x / tileResolution) % tilesPerSide;
            const bool useLight = ((tileX + tileY) % 2U) == 0U;
            const auto& colour = useLight ? lightColour : darkColour;
            const std::size_t index = (static_cast<std::size_t>(y) * resolution * 4U) + (static_cast<std::size_t>(x) * 4U);
            pixels[index + 0U] = srgb_encode(colour[0]);
            pixels[index + 1U] = srgb_encode(colour[1]);
            pixels[index + 2U] = srgb_encode(colour[2]);
        }
    }

    return pixels;
}

auto generate_normal_map_rgba(std::uint32_t resolution, std::uint32_t seed, float amplitude) -> std::vector<std::uint8_t> {
    resolution = validate_resolution(resolution);
    if(amplitude <= 0.0F) {
        throw std::invalid_argument {"Normal map amplitude must be greater than zero"};
    }

    const float frequency = 4.0F;
    const float scale = frequency / static_cast<float>(resolution);
    const std::size_t pixelCount = static_cast<std::size_t>(resolution) * static_cast<std::size_t>(resolution);
    std::vector<std::uint8_t> pixels(pixelCount * 4U, 255U);

    for(std::uint32_t y {0U}; y < resolution; ++y) {
        for(std::uint32_t x {0U}; x < resolution; ++x) {
            const float sampleX = static_cast<float>(x) * scale;
            const float sampleY = static_cast<float>(y) * scale;

    const float noiseX = (value_noise(sampleX, sampleY, seed) * 2.0F) - 1.0F;
    const float noiseY = (value_noise(sampleX, sampleY, seed + 1U) * 2.0F) - 1.0F;

            glm::vec3 normal {noiseX * amplitude, noiseY * amplitude, 1.0F};
            if(glm::dot(normal, normal) <= std::numeric_limits<float>::epsilon()) {
                normal = glm::vec3 {0.0F, 0.0F, 1.0F};
            } else {
                normal = glm::normalize(normal);
            }

            const auto packed = pack_normal(normal);
            const std::size_t index = (static_cast<std::size_t>(y) * resolution * 4U) + (static_cast<std::size_t>(x) * 4U);
            pixels[index + 0U] = packed[0];
            pixels[index + 1U] = packed[1];
            pixels[index + 2U] = packed[2];
        }
    }
    return pixels;
}

} // namespace vulkano
