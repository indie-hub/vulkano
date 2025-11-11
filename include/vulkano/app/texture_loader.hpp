#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include <glm/vec4.hpp>

#include <vulkano/scene/material.hpp>

namespace vulkano::app {
enum class TextureColorSpace : std::uint32_t {
    sRGB = 0U,
    Linear = 1U
};

enum class TextureChannels : std::uint32_t {
    Gray = 1U,
    GrayAlpha = 2U,
    RGB = 3U,
    RGBA = 4U
};

[[nodiscard]] constexpr std::uint32_t channel_count(TextureChannels channels) noexcept {
    return static_cast<std::uint32_t>(channels);
}

struct TextureData final {
    std::vector<std::uint8_t> pixels {};
    std::uint32_t width {0U};
    std::uint32_t height {0U};
    TextureChannels channels {TextureChannels::RGBA};
    TextureColorSpace colorSpace {TextureColorSpace::sRGB};
};

[[nodiscard]] TextureData load_texture_from_file(const std::filesystem::path& path,
    TextureColorSpace colorSpace, bool flipVertical = true);

[[nodiscard]] TextureData make_solid_texture(const glm::vec4& color, TextureColorSpace colorSpace);

[[nodiscard]] TextureData make_checkerboard_texture(const glm::vec4& colorA, const glm::vec4& colorB,
    std::uint32_t resolution, TextureColorSpace colorSpace);
}

