#include <catch2/catch_test_macros.hpp>
#include <vulkano/app/ssao.hpp>

#include <cmath>
#include <glm/geometric.hpp>

namespace {
constexpr float epsilon {1e-4F};
constexpr std::size_t kernelSize {32U};
constexpr std::size_t noiseSize {16U};
}

TEST_CASE("SSAO kernel remains inside unit hemisphere") {
    const vulkano::app::SSAOSampleGenerator generator {};
    const auto samples = generator.generate_kernel(kernelSize);

    REQUIRE(samples.size() == kernelSize);

    float accumulatorFirstHalf {0.0F};
    float accumulatorSecondHalf {0.0F};

    for (std::size_t i {0U}; i < samples.size(); ++i) {
        const glm::vec3& s = samples[i];
        REQUIRE(s.z >= 0.0F);
        const float length = glm::length(s);
        REQUIRE(length <= 1.0F + epsilon);
        if (i < samples.size() / 2U) {
            accumulatorFirstHalf += length;
        } else {
            accumulatorSecondHalf += length;
        }
    }

    const float firstAverage = accumulatorFirstHalf / static_cast<float>(samples.size() / 2U);
    const float secondAverage = accumulatorSecondHalf / static_cast<float>(samples.size() - samples.size() / 2U);
    REQUIRE(secondAverage > firstAverage);
}

TEST_CASE("SSAO noise is tangent-space and deterministic") {
    const vulkano::app::SSAOSampleGenerator generator {42U};
    const auto noise = generator.generate_noise(noiseSize);

    REQUIRE(noise.size() == noiseSize);

    const auto noiseRepeat = generator.generate_noise(noiseSize);
    REQUIRE(noiseRepeat == noise);

    for (const glm::vec3& n : noise) {
        REQUIRE(std::abs(n.z) <= epsilon);
        REQUIRE(glm::length(glm::vec2 {n.x, n.y}) <= 1.0F + epsilon);
    }
}
