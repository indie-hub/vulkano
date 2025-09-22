#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkano/cascaded_shadow_map.hpp>

namespace {
    constexpr float epsilon {1e-4F};
}

using Catch::Approx;

TEST_CASE("compute_cascade_splits produces ascending distances", "[shadow][cascades]") {
    const float nearPlane {0.1F};
    const float farPlane {100.0F};
    const std::uint32_t cascadeCount {4U};
    const float lambda {0.5F};

    const auto splits = vulkano::detail::compute_cascade_splits(nearPlane, farPlane, cascadeCount, lambda);

    REQUIRE(splits.front() > nearPlane);
    for(std::size_t index {1U}; index < cascadeCount; ++index) {
        INFO("cascade index " << index);
        REQUIRE(splits.at(index) >= splits.at(index - 1U));
    }
    REQUIRE(splits.at(cascadeCount - 1U) == Approx(farPlane).margin(1e-3F));
}

TEST_CASE("compute_cascaded_shadow_data encodes settings", "[shadow][uniform]") {
    vulkano::CascadedShadowSettings settings {};
    settings.enabled = true;
    settings.visualizeCascades = true;
    settings.showShadowAtlas = true;
    settings.stabilize = true;
    settings.cascadeCount = 4U;
    settings.resolution = 2048U;

    vulkano::ShadowComputationInput input {};
    input.view = glm::lookAt(
        glm::vec3 {0.0F, 2.0F, 6.0F},
        glm::vec3 {0.0F, 0.0F, 0.0F},
        glm::vec3 {0.0F, 1.0F, 0.0F});
    input.projection = glm::perspective(glm::radians(60.0F), 16.0F / 9.0F, 0.1F, 100.0F);
    input.lightDirection = glm::normalize(glm::vec3 {-1.0F, -1.5F, -0.5F});
    input.nearPlane = 0.1F;
    input.farPlane = 100.0F;

    const vulkano::ShadowCascadeData data = vulkano::compute_cascaded_shadow_data(
        input,
        settings,
        settings.cascadeCount,
        settings.resolution);

    const auto& uniform = data.uniform;
    REQUIRE(uniform.shadowParams.x == Approx(1.0F).margin(epsilon));
    REQUIRE(uniform.shadowParams.w == Approx(static_cast<float>(settings.cascadeCount)).margin(epsilon));
    REQUIRE(uniform.debugParams.x == Approx(1.0F).margin(epsilon));
    REQUIRE(uniform.debugParams.y == Approx(1.0F).margin(epsilon));
    REQUIRE(uniform.debugParams.z == Approx(1.0F).margin(epsilon));

    for(std::size_t index {1U}; index < settings.cascadeCount; ++index) {
        INFO("split order " << index);
        const int current = static_cast<int>(index);
        const int previous = current - 1;
        REQUIRE(uniform.cascadeSplits[current] >= uniform.cascadeSplits[previous]);
    }

    bool anyNonIdentity {false};
    for(std::size_t index {0U}; index < settings.cascadeCount; ++index) {
        const glm::mat4 matrix = uniform.lightViewProjection.at(index);
        bool isIdentity {true};
        for(int row {0}; row < 4 && isIdentity; ++row) {
            for(int col {0}; col < 4; ++col) {
                const float expected = (row == col) ? 1.0F : 0.0F;
                if(std::abs(matrix[row][col] - expected) > 1e-3F) {
                    isIdentity = false;
                    break;
                }
            }
        }
        if(!isIdentity) {
            anyNonIdentity = true;
        }
    }
    REQUIRE(anyNonIdentity);
}

TEST_CASE("disabled cascades zero shadow flag", "[shadow][uniform]") {
    vulkano::CascadedShadowSettings settings {};
    settings.enabled = false;
    settings.cascadeCount = 3U;
    settings.resolution = 1024U;

    vulkano::ShadowComputationInput input {};
    input.nearPlane = 0.5F;
    input.farPlane = 50.0F;

    const vulkano::ShadowCascadeData data = vulkano::compute_cascaded_shadow_data(
        input,
        settings,
        settings.cascadeCount,
        settings.resolution);

    REQUIRE(data.uniform.shadowParams.x == Approx(0.0F).margin(epsilon));
    REQUIRE(data.uniform.shadowParams.w == Approx(static_cast<float>(settings.cascadeCount)).margin(epsilon));
}
