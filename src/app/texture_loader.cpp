#include <vulkano/app/texture_loader.hpp>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_NO_PSD
#define STBI_NO_GIF
#define STBI_NO_PIC
#define STBI_NO_PNM
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wunused-function"
#endif
#include <third_party/stb/stb_image.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

namespace vulkano::app {
namespace {
[[nodiscard]] std::uint32_t clamp_resolution(std::uint32_t value) noexcept {
    return std::max(1U, value);
}

[[nodiscard]] TextureData make_placeholder_impl(const glm::vec4& colorA, const glm::vec4& colorB,
    std::uint32_t resolution, TextureColorSpace colorSpace, bool checkerboard) {
    const std::uint32_t clampedResolution = clamp_resolution(resolution);
    TextureData data {};
    data.width = clampedResolution;
    data.height = clampedResolution;
    data.channels = TextureChannels::RGBA;
    data.colorSpace = colorSpace;
    data.pixels.resize(static_cast<std::size_t>(data.width) * static_cast<std::size_t>(data.height) * 4U);

    const std::uint32_t blockSize = 1U;

    for (std::uint32_t y {0U}; y < data.height; ++y) {
        for (std::uint32_t x {0U}; x < data.width; ++x) {
            const bool useSecond = checkerboard
                ? ((x / blockSize) + (y / blockSize)) % 2U == 1U
                : false;
            const glm::vec4 selected = useSecond ? colorB : colorA;
            const std::size_t index = (static_cast<std::size_t>(y) * static_cast<std::size_t>(data.width)
                + static_cast<std::size_t>(x))
                * 4U;
            data.pixels[index + 0U] = static_cast<std::uint8_t>(std::clamp(selected.r, 0.0F, 1.0F) * 255.0F + 0.5F);
            data.pixels[index + 1U] = static_cast<std::uint8_t>(std::clamp(selected.g, 0.0F, 1.0F) * 255.0F + 0.5F);
            data.pixels[index + 2U] = static_cast<std::uint8_t>(std::clamp(selected.b, 0.0F, 1.0F) * 255.0F + 0.5F);
            data.pixels[index + 3U] = static_cast<std::uint8_t>(std::clamp(selected.a, 0.0F, 1.0F) * 255.0F + 0.5F);
        }
    }

    return data;
}
} // namespace

TextureData load_texture_from_file(const std::filesystem::path& path, TextureColorSpace colorSpace, bool flipVertical) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error {"Texture file not found: " + path.string()};
    }

    stbi_set_flip_vertically_on_load(flipVertical ? 1 : 0);
    int width {0};
    int height {0};
    int components {0};
    constexpr int desiredComponents {4};
    stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &components, desiredComponents);
    if (pixels == nullptr) {
        throw std::runtime_error {"Failed to load texture: " + std::string {stbi_failure_reason()}};
    }

    TextureData data {};
    data.width = static_cast<std::uint32_t>(width);
    data.height = static_cast<std::uint32_t>(height);
    data.channels = TextureChannels::RGBA;
    data.colorSpace = colorSpace;
    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * desiredComponents;
    data.pixels.assign(pixels, pixels + pixelCount);
    stbi_image_free(pixels);

    return data;
}

TextureData make_solid_texture(const glm::vec4& color, TextureColorSpace colorSpace) {
    return make_placeholder_impl(color, color, 1U, colorSpace, false);
}

TextureData make_checkerboard_texture(const glm::vec4& colorA, const glm::vec4& colorB,
    std::uint32_t resolution, TextureColorSpace colorSpace) {
    return make_placeholder_impl(colorA, colorB, resolution, colorSpace, true);
}
} // namespace vulkano::app
