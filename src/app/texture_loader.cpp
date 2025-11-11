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

#include <third_party/lodepng/lodepng.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace vulkano::app {
namespace {
[[nodiscard]] bool should_flip_vertically() noexcept {
    if (const char* disableFlip = std::getenv("VULKANO_DISABLE_TEXTURE_FLIP")) {
        if (std::strcmp(disableFlip, "0") != 0) {
            return false;
        }
    }
    return true;
}

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
    const bool flip = flipVertical && should_flip_vertically();
    stbi_set_flip_vertically_on_load(flip ? 1 : 0);
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

TextureData load_texture_from_memory(const std::uint8_t* data, std::size_t size, TextureColorSpace colorSpace,
    bool flipVertical, std::string_view formatHint) {
    if (data == nullptr || size == 0U) {
        throw std::invalid_argument {"Embedded texture data is empty"};
    }
    const bool flip = flipVertical && should_flip_vertically();
    stbi_set_flip_vertically_on_load(flip ? 1 : 0);
    int width {0};
    int height {0};
    int components {0};
    constexpr int desiredComponents {0};
    stbi_uc* pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(data), static_cast<int>(size), &width,
        &height, &components, desiredComponents);
    if (pixels == nullptr) {
        const char* reason = stbi_failure_reason();
        std::string failure = reason != nullptr ? reason : "unknown reason";

        auto is_png_hint = [&]() {
            if (formatHint.empty()) {
                return false;
            }
            if (formatHint.size() != 3U) {
                return false;
            }
            return (formatHint[0] == 'p' || formatHint[0] == 'P')
                && (formatHint[1] == 'n' || formatHint[1] == 'N')
                && (formatHint[2] == 'g' || formatHint[2] == 'G');
        };

        if (is_png_hint()) {
            unsigned int decodedWidth {0U};
            unsigned int decodedHeight {0U};
            std::vector<unsigned char> decoded;
            lodepng::State state {};
            state.decoder.ignore_crc = 1U;
            state.decoder.zlibsettings.ignore_adler32 = 1U;
            const unsigned int error = lodepng::decode(decoded, decodedWidth, decodedHeight, state, data, size);
            if (error == 0U) {
                TextureData texture {};
                texture.width = decodedWidth;
                texture.height = decodedHeight;
                texture.channels = TextureChannels::RGBA;
                texture.colorSpace = colorSpace;
                texture.pixels = std::move(decoded);
                if (flipVertical) {
                    const std::size_t rowBytes = static_cast<std::size_t>(texture.width) * 4U;
                    std::vector<std::uint8_t> row(rowBytes);
                    for (std::uint32_t y {0U}; y < texture.height / 2U; ++y) {
                        std::uint8_t* top = texture.pixels.data() + static_cast<std::size_t>(y) * rowBytes;
                        std::uint8_t* bottom = texture.pixels.data()
                            + static_cast<std::size_t>(texture.height - 1U - y) * rowBytes;
                        std::memcpy(row.data(), top, rowBytes);
                        std::memcpy(top, bottom, rowBytes);
                        std::memcpy(bottom, row.data(), rowBytes);
                    }
                }
                return texture;
            }
            failure = std::string {"lodepng decode error "} + std::to_string(error) + ": " + lodepng_error_text(error);
        }

        throw std::runtime_error {"Failed to load embedded texture: " + failure};
    }

    TextureData texture {};
    texture.width = static_cast<std::uint32_t>(width);
    texture.height = static_cast<std::uint32_t>(height);
    texture.colorSpace = colorSpace;
    texture.channels = TextureChannels::RGBA;
    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    texture.pixels.resize(pixelCount * 4U);

    const auto convert = [&](auto&& setter) {
        for (std::size_t idx {0U}; idx < pixelCount; ++idx) {
            setter(idx);
        }
    };

    if (components == 4) {
        texture.pixels.assign(pixels, pixels + pixelCount * 4U);
    } else if (components == 3) {
        convert([&](std::size_t idx) {
            texture.pixels[idx * 4U + 0U] = pixels[idx * 3U + 0U];
            texture.pixels[idx * 4U + 1U] = pixels[idx * 3U + 1U];
            texture.pixels[idx * 4U + 2U] = pixels[idx * 3U + 2U];
            texture.pixels[idx * 4U + 3U] = 255U;
        });
    } else if (components == 2) {
        convert([&](std::size_t idx) {
            const stbi_uc luminance = pixels[idx * 2U + 0U];
            const stbi_uc alpha = pixels[idx * 2U + 1U];
            texture.pixels[idx * 4U + 0U] = luminance;
            texture.pixels[idx * 4U + 1U] = luminance;
            texture.pixels[idx * 4U + 2U] = luminance;
            texture.pixels[idx * 4U + 3U] = alpha;
        });
    } else if (components == 1) {
        convert([&](std::size_t idx) {
            const stbi_uc luminance = pixels[idx];
            texture.pixels[idx * 4U + 0U] = luminance;
            texture.pixels[idx * 4U + 1U] = luminance;
            texture.pixels[idx * 4U + 2U] = luminance;
            texture.pixels[idx * 4U + 3U] = 255U;
        });
    } else {
        stbi_image_free(pixels);
        throw std::runtime_error {"Unsupported embedded texture component count"};
    }
    stbi_image_free(pixels);

    return texture;
}

TextureData make_solid_texture(const glm::vec4& color, TextureColorSpace colorSpace) {
    return make_placeholder_impl(color, color, 1U, colorSpace, false);
}

TextureData make_checkerboard_texture(const glm::vec4& colorA, const glm::vec4& colorB,
    std::uint32_t resolution, TextureColorSpace colorSpace) {
    return make_placeholder_impl(colorA, colorB, resolution, colorSpace, true);
}
} // namespace vulkano::app
