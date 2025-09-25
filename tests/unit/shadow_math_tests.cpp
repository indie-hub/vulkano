#include <catch2/catch_all.hpp>

#include <vulkano/camera_math.hpp>

#include <glm/gtc/matrix_transform.hpp>

namespace {
    constexpr float epsilon {1e-4F};

    void expect_matrices_close(const glm::mat4& lhs, const glm::mat4& rhs) {
        for(int row = 0; row < 4; ++row) {
            for(int col = 0; col < 4; ++col) {
                REQUIRE(lhs[row][col] == Catch::Approx(rhs[row][col]).margin(1e-4F));
            }
        }
    }
}

TEST_CASE("compute_light_view_projection returns expected matrix when target differs from position") {
    const glm::vec3 lightPosition {5.0F, 6.0F, 7.0F};
    const glm::vec3 target {0.0F, 0.0F, 0.0F};
    const glm::vec3 worldUp {0.0F, 1.0F, 0.0F};
    const glm::vec3 fallbackDirection {0.0F, -1.0F, 0.0F};
    const float fovY {glm::radians(60.0F)};
    const float nearPlane {0.1F};
    const float farPlane {50.0F};

    const glm::mat4 expected = glm::perspective(fovY, 1.0F, nearPlane, farPlane)
        * glm::lookAt(lightPosition, target, worldUp);
    const glm::mat4 actual = vulkano::compute_light_view_projection(
        lightPosition,
        target,
        worldUp,
        fovY,
        nearPlane,
        farPlane,
        fallbackDirection,
        epsilon);

    expect_matrices_close(actual, expected);
}

TEST_CASE("compute_light_view_projection uses fallback direction when target matches light position") {
    const glm::vec3 lightPosition {0.0F, 5.0F, 0.0F};
    const glm::vec3 target {0.0F, 5.0F, 0.0F};
    const glm::vec3 worldUp {0.0F, 1.0F, 0.0F};
    const glm::vec3 fallbackDirection {0.0F, -1.0F, 0.0F};
    const float fovY {glm::radians(45.0F)};
    const float nearPlane {0.5F};
    const float farPlane {100.0F};

    const glm::vec3 expectedDirection {0.0F, 0.0F, -1.0F};
    const glm::mat4 expected = glm::perspective(fovY, 1.0F, nearPlane, farPlane)
        * glm::lookAt(lightPosition, lightPosition + expectedDirection, worldUp);
    const glm::mat4 actual = vulkano::compute_light_view_projection(
        lightPosition,
        target,
        worldUp,
        fovY,
        nearPlane,
        farPlane,
        fallbackDirection,
        epsilon);

    expect_matrices_close(actual, expected);
}

TEST_CASE("compute_cascade_splits blends linear and logarithmic distributions") {
    const float nearPlane {0.5F};
    const float farPlane {50.0F};
    constexpr std::uint32_t cascadeCount {3U};
    const float lambda {0.6F};

    const auto splits = vulkano::compute_cascade_splits(nearPlane, farPlane, cascadeCount, lambda);

    REQUIRE(splits[0] > 0.0F);
    REQUIRE(splits[0] < splits[1]);
    REQUIRE(splits[1] < splits[2]);
    REQUIRE(splits[2] <= 1.0F);
}

TEST_CASE("select_cascade clamps depth outside split range") {
    const std::array<float, vulkano::maxShadowCascades> splits {0.1F, 0.3F, 0.6F, 1.0F};
    constexpr std::uint32_t cascadeCount {4U};

    REQUIRE(vulkano::select_cascade(-0.5F, splits, cascadeCount) == 0U);
    REQUIRE(vulkano::select_cascade(0.15F, splits, cascadeCount) == 1U);
    REQUIRE(vulkano::select_cascade(0.45F, splits, cascadeCount) == 2U);
    REQUIRE(vulkano::select_cascade(0.9F, splits, cascadeCount) == 3U);
    REQUIRE(vulkano::select_cascade(2.0F, splits, cascadeCount) == 3U);
}
