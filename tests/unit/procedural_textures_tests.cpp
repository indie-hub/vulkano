#include <catch2/catch_test_macros.hpp>

#include <vulkano/procedural_textures.hpp>

#include <cstdint>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

namespace {
    [[nodiscard]] auto decode_normal(std::uint8_t r, std::uint8_t g, std::uint8_t b) -> glm::vec3 {
        const float nx = (static_cast<float>(r) / 255.0F * 2.0F) - 1.0F;
        const float ny = (static_cast<float>(g) / 255.0F * 2.0F) - 1.0F;
        const float nz = (static_cast<float>(b) / 255.0F * 2.0F) - 1.0F;
        return glm::vec3 {nx, ny, nz};
    }
}

TEST_CASE("Checkerboard generator outputs expected pattern", "[procedural_textures]") {
    constexpr std::uint32_t resolution {8U};
    constexpr std::uint32_t tilesPerSide {2U};

    const auto pixels = vulkano::generate_checkerboard_rgba_srgb(resolution, tilesPerSide);
    REQUIRE(pixels.size() == static_cast<std::size_t>(resolution * resolution * 4U));

    const auto& topLeft = pixels;
    REQUIRE(topLeft[0] == 204U);
    REQUIRE(topLeft[1] == 204U);
    REQUIRE(topLeft[2] == 204U);
    REQUIRE(topLeft[3] == 255U);

    const std::size_t offset = (resolution / tilesPerSide) * 4U;
    REQUIRE(offset < pixels.size());
    REQUIRE(pixels[offset + 0] == 51U);
    REQUIRE(pixels[offset + 1] == 51U);
    REQUIRE(pixels[offset + 2] == 51U);
    REQUIRE(pixels[offset + 3] == 255U);
}

TEST_CASE("Normal map generator normalizes output", "[procedural_textures]") {
    constexpr std::uint32_t resolution {4U};
    constexpr std::uint32_t seed {1234U};
    constexpr float amplitude {0.5F};

    const auto pixels = vulkano::generate_normal_map_rgba(resolution, seed, amplitude);
    REQUIRE(pixels.size() == static_cast<std::size_t>(resolution * resolution * 4U));

    for(std::size_t index {0U}; index < pixels.size(); index += 4U) {
        const glm::vec3 normal = decode_normal(pixels[index + 0U], pixels[index + 1U], pixels[index + 2U]);
        const float lengthSquared = glm::dot(normal, normal);
        REQUIRE(lengthSquared >= 0.70F);
        REQUIRE(lengthSquared <= 1.05F);
        REQUIRE(pixels[index + 3U] == 255U);
    }
}
