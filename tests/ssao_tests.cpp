#include <catch2/catch_all.hpp>

#include <vulkano/ssao.hpp>

TEST_CASE("SSAO kernel sizes and hemisphere") {
    const std::uint32_t sizes[] {16U, 32U, 64U};
    for (std::uint32_t s : sizes) {
        auto k = vulkano::generate_ssao_kernel(s);
        REQUIRE(k.size() == s);
        for (const auto& v : k) {
            // Hemisphere +Z
            REQUIRE(v.z >= 0.0F);
            // Magnitude not exceeding 1
            const float len = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
            REQUIRE(len <= Catch::Approx(1.0F).margin(1e-4F));
        }
    }
}

TEST_CASE("SSAO noise vectors unit length and distribution") {
    auto noise = vulkano::generate_ssao_noise_vectors(64U);
    REQUIRE(noise.size() == 64U);
    // Should be roughly normalized and within [-1,1]
    for (const auto& p : noise) {
        const float len = std::sqrt(p.x*p.x + p.y*p.y);
        REQUIRE(len <= Catch::Approx(1.0F).margin(1e-4F));
        REQUIRE(p.x >= -1.0F);
        REQUIRE(p.x <=  1.0F);
        REQUIRE(p.y >= -1.0F);
        REQUIRE(p.y <=  1.0F);
    }
}

