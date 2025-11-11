#include <catch2/catch_test_macros.hpp>

#include <vulkano/app/texture_loader.hpp>

#include <array>
#include <filesystem>
#include <fstream>

#include <glm/vec4.hpp>

namespace {
constexpr std::array<unsigned char, 106> kRedPng {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
    0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01, 0x01, 0x03, 0x00, 0x00, 0x00, 0x25, 0xDB, 0x56,
    0xCA, 0x00, 0x00, 0x00, 0x06, 0x50, 0x4C, 0x54, 0x45, 0xFF, 0x00, 0x00,
    0xFF, 0x00, 0x00, 0xFF, 0x3B, 0x26, 0xEE, 0x00, 0x00, 0x00, 0x09, 0x70,
    0x48, 0x59, 0x73, 0x00, 0x00, 0x0E, 0xC4, 0x00, 0x00, 0x0E, 0xC4, 0x01,
    0x95, 0x2B, 0x0E, 0x1B, 0x00, 0x00, 0x00, 0x0A, 0x49, 0x44, 0x41, 0x54,
    0x08, 0x99, 0x63, 0x60, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0xF4, 0x71,
    0x64, 0xA6, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42,
    0x60, 0x82
};
}

TEST_CASE("Solid texture produces expected pixel data") {
    const glm::vec4 magenta {1.0F, 0.0F, 1.0F, 0.5F};
    const auto texture = vulkano::app::make_solid_texture(magenta, vulkano::app::TextureColorSpace::sRGB);

    REQUIRE(texture.width == 1U);
    REQUIRE(texture.height == 1U);
    REQUIRE(texture.channels == vulkano::app::TextureChannels::RGBA);
    REQUIRE(texture.pixels.size() == 4U);
    REQUIRE(texture.pixels[0U] == 255U);
    REQUIRE(texture.pixels[1U] == 0U);
    REQUIRE(texture.pixels[2U] == 255U);
    REQUIRE(texture.pixels[3U] == 128U);
}

TEST_CASE("Checkerboard texture alternates colours") {
    const glm::vec4 white {1.0F, 1.0F, 1.0F, 1.0F};
    const glm::vec4 black {0.0F, 0.0F, 0.0F, 1.0F};
    const auto texture = vulkano::app::make_checkerboard_texture(white, black, 4U, vulkano::app::TextureColorSpace::sRGB);

    REQUIRE(texture.width == 4U);
    REQUIRE(texture.height == 4U);
    REQUIRE(texture.pixels.size() == 4U * 4U * 4U);

    const auto at = [&](std::uint32_t x, std::uint32_t y) {
        const std::size_t index = (static_cast<std::size_t>(y) * 4U + static_cast<std::size_t>(x)) * 4U;
        return std::array<unsigned char, 4> {
            texture.pixels[index + 0U],
            texture.pixels[index + 1U],
            texture.pixels[index + 2U],
            texture.pixels[index + 3U]
        };
    };

    REQUIRE(at(0U, 0U)[0U] == 255U);
    REQUIRE(at(1U, 0U)[0U] == 0U);
    REQUIRE(at(0U, 1U)[0U] == 0U);
}

TEST_CASE("PNG file loads into texture data") {
    const std::filesystem::path temp = std::filesystem::temp_directory_path() / "vulkano_test_red.png";
    {
        std::ofstream file {temp, std::ios::binary};
        file.write(reinterpret_cast<const char*>(kRedPng.data()), static_cast<std::streamsize>(kRedPng.size()));
    }

    const auto texture = vulkano::app::load_texture_from_file(temp, vulkano::app::TextureColorSpace::sRGB, false);
    REQUIRE(texture.width == 1U);
    REQUIRE(texture.height == 1U);
    REQUIRE(texture.pixels.size() == 4U);
    REQUIRE(texture.pixels[0U] == 255U);
    REQUIRE(texture.pixels[3U] == 255U);

    std::filesystem::remove(temp);
}

TEST_CASE("Missing file raises exception") {
    const std::filesystem::path bogus = std::filesystem::temp_directory_path() / "missing_texture.png";
    REQUIRE_THROWS_AS(vulkano::app::load_texture_from_file(bogus, vulkano::app::TextureColorSpace::sRGB), std::runtime_error);
}
