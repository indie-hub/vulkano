#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

#include <vulkano/app/math.hpp>

namespace {
void expect_matrix_close(const glm::mat4& actual, const glm::mat4& expected, float epsilon) {
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            INFO("Mismatch at [" << column << "][" << row << "]");
            REQUIRE(actual[column][row] == Catch::Approx(expected[column][row]).margin(epsilon));
        }
    }
}
}

TEST_CASE("make_triangle_transforms produces right-handed matrices") {
    constexpr float aspectRatio {16.0F / 9.0F};
    const vulkano::app::TriangleTransforms transforms {vulkano::app::make_triangle_transforms(aspectRatio)};

    const glm::mat4 expectedModel {1.0F};
    const glm::mat4 expectedView {glm::lookAtRH(glm::vec3 {0.0F, 0.0F, 2.0F}, glm::vec3 {0.0F, 0.0F, 0.0F}, glm::vec3 {0.0F, 1.0F, 0.0F})};
    glm::mat4 expectedProjection {glm::perspectiveRH_ZO(glm::radians(60.0F), aspectRatio, 0.1F, 10.0F)};
    expectedProjection[1][1] *= -1.0F;

    expect_matrix_close(transforms.model, expectedModel, 1e-5F);
    expect_matrix_close(transforms.view, expectedView, 1e-5F);
    expect_matrix_close(transforms.projection, expectedProjection, 1e-5F);

    REQUIRE(transforms.projection[1][1] < 0.0F);
}
