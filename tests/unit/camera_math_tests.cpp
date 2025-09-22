#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <vulkano/camera_math.hpp>

using Catch::Approx;

namespace {
    constexpr float tolerance {1e-4F};
}

TEST_CASE("compute_forward returns expected direction", "[camera]") {
    const glm::vec3 forward = vulkano::compute_forward(0.0F, 0.0F);
    REQUIRE(forward.x == Approx(1.0F).margin(tolerance));
    REQUIRE(forward.y == Approx(0.0F).margin(tolerance));
    REQUIRE(forward.z == Approx(0.0F).margin(tolerance));
}

TEST_CASE("clamp_pitch limits angle", "[camera]") {
    const float minPitch = -glm::radians(89.0F);
    const float maxPitch = glm::radians(89.0F);
    REQUIRE(vulkano::clamp_pitch(glm::radians(120.0F), minPitch, maxPitch) == Approx(maxPitch).margin(tolerance));
    REQUIRE(vulkano::clamp_pitch(glm::radians(-120.0F), minPitch, maxPitch) == Approx(minPitch).margin(tolerance));
}

TEST_CASE("adjust_fov clamps within range", "[camera]") {
    const float minFov = glm::radians(35.0F);
    const float maxFov = glm::radians(90.0F);
    const float initial = glm::radians(60.0F);

    SECTION("scrolling in reduces fov") {
        const float updated = vulkano::adjust_fov(initial, glm::radians(5.0F), minFov, maxFov);
        REQUIRE(updated == Approx(glm::radians(55.0F)).margin(tolerance));
    }

    SECTION("clamped to minimum") {
        const float updated = vulkano::adjust_fov(initial, glm::radians(100.0F), minFov, maxFov);
        REQUIRE(updated == Approx(minFov).margin(tolerance));
    }

    SECTION("clamped to maximum when scrolling out") {
        const float updated = vulkano::adjust_fov(initial, glm::radians(-100.0F), minFov, maxFov);
        REQUIRE(updated == Approx(maxFov).margin(tolerance));
    }
}

TEST_CASE("compute_view_matrix matches glm lookAt", "[camera]") {
    const glm::vec3 position {2.0F, 1.0F, -3.0F};
    const float yaw = glm::radians(-45.0F);
    const float pitch = glm::radians(15.0F);
    const glm::vec3 up {0.0F, 1.0F, 0.0F};
    const glm::vec3 forward = vulkano::compute_forward(yaw, pitch);
    const glm::vec3 right = vulkano::compute_right(forward, up);
    const glm::vec3 correctedUp = vulkano::compute_up(forward, right);
    const glm::mat4 expected = glm::lookAt(position, position + forward, correctedUp);
    const glm::mat4 view = vulkano::compute_view_matrix(position, yaw, pitch, up);

    for(int row = 0; row < 4; ++row) {
        for(int column = 0; column < 4; ++column) {
            REQUIRE(view[row][column] == Approx(expected[row][column]).margin(tolerance));
        }
    }
}
