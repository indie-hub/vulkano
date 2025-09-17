#include <catch2/catch_all.hpp>

#include <vulkano/textures.hpp>

TEST_CASE("Checkerboard image dimensions and pattern") {
    // Config constants
    constexpr std::uint32_t kSize {256U};
    constexpr std::uint32_t kSquares {8U};
    const glm::vec3 light {0.92F, 0.92F, 0.92F};
    const glm::vec3 dark {0.12F, 0.12F, 0.12F};

    const auto img = vulkano::make_checkerboard_rgba(kSize, kSquares, light, dark);
    REQUIRE(img.width == kSize);
    REQUIRE(img.height == kSize);
    REQUIRE(img.pixels.size() == static_cast<std::size_t>(kSize) * static_cast<std::size_t>(kSize) * 4U);

    // Sample four corners to assert alternating colors
    const auto px = [&](std::uint32_t x, std::uint32_t y) {
        const std::size_t idx {static_cast<std::size_t>((y * kSize + x) * 4U)};
        return std::array<std::uint8_t, 4> {img.pixels[idx+0], img.pixels[idx+1], img.pixels[idx+2], img.pixels[idx+3]};
    };
    const auto p00 = px(0U, 0U);
    const auto p11 = px(kSize/8U, kSize/8U);
    const auto p01 = px(0U, kSize/8U);
    const auto p10 = px(kSize/8U, 0U);

    // Alpha should be opaque
    REQUIRE(p00[3] == 255U);
    REQUIRE(p11[3] == 255U);
    REQUIRE(p01[3] == 255U);
    REQUIRE(p10[3] == 255U);

    // Pattern alternates between two colors
    const bool eq00_11 = (p00[0] == p11[0] && p00[1] == p11[1] && p00[2] == p11[2]);
    const bool eq00_01 = (p00[0] == p01[0] && p00[1] == p01[1] && p00[2] == p01[2]);
    const bool eq00_10 = (p00[0] == p10[0] && p00[1] == p10[1] && p00[2] == p10[2]);
    REQUIRE(eq00_11);
    REQUIRE((!eq00_01 || !eq00_10));
}

TEST_CASE("Blue-noise-ish normal map basics") {
    constexpr std::uint32_t kSize {128U};
    constexpr float kAmp {0.8F};
    const auto img = vulkano::make_blue_noise_normal_rgba(kSize, kAmp);
    REQUIRE(img.width == kSize);
    REQUIRE(img.height == kSize);
    REQUIRE(img.pixels.size() == static_cast<std::size_t>(kSize) * static_cast<std::size_t>(kSize) * 4U);

    // Check statistics: alpha is 255, blue channel biased high (nz), and non-constant nx,ny
    std::size_t count {0U};
    std::size_t alphaOk {0U};
    double meanB {0.0};
    double varR {0.0};
    double varG {0.0};
    double meanR {0.0};
    double meanG {0.0};
    for (std::uint32_t y = 0; y < kSize; ++y) {
        for (std::uint32_t x = 0; x < kSize; ++x) {
            const std::size_t idx {static_cast<std::size_t>((y * kSize + x) * 4U)};
            const std::uint8_t r = img.pixels[idx + 0U];
            const std::uint8_t g = img.pixels[idx + 1U];
            const std::uint8_t b = img.pixels[idx + 2U];
            const std::uint8_t a = img.pixels[idx + 3U];
            alphaOk += (a == 255U) ? 1U : 0U;
            meanB += static_cast<double>(b);
            meanR += static_cast<double>(r);
            meanG += static_cast<double>(g);
            ++count;
        }
    }
    REQUIRE(alphaOk == count);
    meanB /= static_cast<double>(count);
    meanR /= static_cast<double>(count);
    meanG /= static_cast<double>(count);

    // Blue channel should be noticeably high (nz near 1)
    REQUIRE(meanB > 170.0);

    // Check there is variation: compute simple variance proxies
    for (std::uint32_t y = 0; y < kSize; ++y) {
        for (std::uint32_t x = 0; x < kSize; ++x) {
            const std::size_t idx {static_cast<std::size_t>((y * kSize + x) * 4U)};
            const std::uint8_t r = img.pixels[idx + 0U];
            const std::uint8_t g = img.pixels[idx + 1U];
            varR += (static_cast<double>(r) - meanR) * (static_cast<double>(r) - meanR);
            varG += (static_cast<double>(g) - meanG) * (static_cast<double>(g) - meanG);
        }
    }
    varR /= static_cast<double>(count);
    varG /= static_cast<double>(count);
    REQUIRE(varR > 50.0);
    REQUIRE(varG > 50.0);
}

