#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <array>
#include <cmath>
#include <cstdint>

#include <vulkano/texture.hpp>

using Catch::Approx;

TEST_CASE("Checkerboard generator produces alternating texels", "[texture]") {
    const vulkano::TexturePixelData data = vulkano::generate_checkerboard_pixels();

    REQUIRE(data.extent.width == 256U);
    REQUIRE(data.extent.height == 256U);
    REQUIRE(data.pixels.size() == static_cast<std::size_t>(data.extent.width) * data.extent.height * 4U);

    const auto lightValue = static_cast<std::uint8_t>(std::lround(0.92F * 255.0F));
    const auto darkValue = static_cast<std::uint8_t>(std::lround(0.12F * 255.0F));

    const auto texel = [&data](std::uint32_t x, std::uint32_t y) {
        const std::size_t index = (static_cast<std::size_t>(y) * data.extent.width + x) * 4U;
        return std::array<std::uint8_t, 4U> {
            data.pixels.at(index + 0U),
            data.pixels.at(index + 1U),
            data.pixels.at(index + 2U),
            data.pixels.at(index + 3U)
        };
    };

    const auto topLeft = texel(0U, 0U);
    const auto neighbour = texel(33U, 0U);
    const auto diagonal = texel(33U, 33U);

    CHECK(topLeft[0] == lightValue);
    CHECK(topLeft[1] == lightValue);
    CHECK(topLeft[2] == lightValue);
    CHECK(topLeft[3] == 255U);

    CHECK(neighbour[0] == darkValue);
    CHECK(diagonal[0] == lightValue);
}

TEST_CASE("Blue-noise normal map pixels form unit vectors", "[texture]") {
    const vulkano::TexturePixelData data = vulkano::generate_blue_noise_normal_pixels();

    REQUIRE(data.extent.width == 128U);
    REQUIRE(data.extent.height == 128U);
    REQUIRE(data.pixels.size() == static_cast<std::size_t>(data.extent.width) * data.extent.height * 4U);

    const auto decode_component = [](std::uint8_t value) -> float {
        return (static_cast<float>(value) / 255.0F) * 2.0F - 1.0F;
    };

    double averageLength {0.0};
    std::size_t sampleCount {0U};

    for(std::uint32_t y {0U}; y < data.extent.height; ++y) {
        for(std::uint32_t x {0U}; x < data.extent.width; ++x) {
            const std::size_t index = (static_cast<std::size_t>(y) * data.extent.width + x) * 4U;
            const float nx = decode_component(data.pixels.at(index + 0U));
            const float ny = decode_component(data.pixels.at(index + 1U));
            const float nz = decode_component(data.pixels.at(index + 2U));
            const float length = std::sqrt((nx * nx) + (ny * ny) + (nz * nz));
            averageLength += static_cast<double>(length);
            ++sampleCount;

            CHECK(length == Approx(1.0F).margin(0.15F));
            CHECK(nz >= -0.02F);
        }
    }

    REQUIRE(sampleCount > 0U);
    const double meanLength = averageLength / static_cast<double>(sampleCount);
    CHECK(meanLength == Approx(1.0).margin(0.02));
}
