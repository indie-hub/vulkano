#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vulkano/app/math.hpp>
#include <vulkano/app/ssao.hpp>

#include <cmath>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>

namespace {
constexpr float epsilon {1e-4F};
constexpr std::size_t kernelSize {32U};
constexpr std::size_t noiseSize {16U};
}

TEST_CASE("Normals transform into view space") {
    const glm::mat4 model = glm::mat4 {1.0F};
    const glm::mat4 view = glm::rotate(glm::mat4 {1.0F}, glm::radians(-90.0F), glm::vec3 {1.0F, 0.0F, 0.0F});
    const glm::vec3 normal {0.0F, 1.0F, 0.0F};

    const glm::vec3 viewNormal = vulkano::app::transform_normal_to_view(model, view, normal);

    using Catch::Matchers::WithinAbs;
    REQUIRE_THAT(viewNormal.x, WithinAbs(0.0F, epsilon));
    REQUIRE_THAT(viewNormal.y, WithinAbs(0.0F, epsilon));
    REQUIRE_THAT(viewNormal.z, WithinAbs(-1.0F, epsilon));
    REQUIRE_THAT(glm::length(viewNormal), WithinAbs(1.0F, epsilon));
}

TEST_CASE("View-space position reconstructs from linear depth") {
    const glm::mat4 projection = []() {
        glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(60.0F), 1.0F, 0.1F, 10.0F);
        proj[1][1] *= -1.0F;
        return proj;
    }();
    const glm::mat4 inverseProjection = glm::inverse(projection);
    const glm::vec3 originalViewPosition {0.5F, -0.25F, -2.5F};

    const glm::vec4 clip = projection * glm::vec4 {originalViewPosition, 1.0F};
    const glm::vec3 ndc = glm::vec3 {clip} / clip.w;
    const glm::vec2 uv {ndc.x * 0.5F + 0.5F, ndc.y * 0.5F + 0.5F};
    const float linearDepth = -originalViewPosition.z;

    const glm::vec3 reconstructed = vulkano::app::reconstruct_view_position_from_depth(inverseProjection, uv, linearDepth);

    using Catch::Matchers::WithinAbs;
    REQUIRE_THAT(reconstructed.x, WithinAbs(originalViewPosition.x, 1e-3F));
    REQUIRE_THAT(reconstructed.y, WithinAbs(originalViewPosition.y, 1e-3F));
    REQUIRE_THAT(reconstructed.z, WithinAbs(originalViewPosition.z, 1e-3F));
}

TEST_CASE("SSAO falloff weighting decreases with distance") {
    const float depthRange {0.2F};
    const float distanceRange {0.5F};
    const float depthDifferenceNear {0.05F};
    const float depthDifferenceFar {0.2F};
    const float distanceNear {0.1F};
    const float distanceFar {0.6F};

    const float depthWeightNear = std::exp(-depthDifferenceNear / depthRange);
    const float depthWeightFar = std::exp(-depthDifferenceFar / depthRange);
    const float distanceWeightNear = std::exp(-distanceNear / distanceRange);
    const float distanceWeightFar = std::exp(-distanceFar / distanceRange);

    REQUIRE(depthWeightNear > depthWeightFar);
    REQUIRE(distanceWeightNear > distanceWeightFar);

    const float combinedNear = depthWeightNear * distanceWeightNear;
    const float combinedFar = depthWeightFar * distanceWeightFar;
    REQUIRE(combinedNear > combinedFar);
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
