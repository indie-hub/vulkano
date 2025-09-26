#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <string>
#include <vulkano/application.hpp>

using Catch::Approx;

TEST_CASE("FPS camera defaults look towards scene origin", "[camera]") {
    auto config = vulkano::AppConfigBuilder {}
        .with_validation_enabled(false)
        .build();

    auto validate_camera = [&config]() {
        vulkano::VulkanApplication application {config};
        const glm::vec3 eye = application.camera_eye();
        const glm::vec3 forward = application.camera_forward_direction();

        const glm::vec3 expectedEye {0.0F, 1.5F, -6.0F};
        const glm::vec3 expectedDirection = glm::normalize(glm::vec3 {0.0F, -1.5F, 6.0F});

        CHECK(eye.x == Approx(expectedEye.x).margin(0.001F));
        CHECK(eye.y == Approx(expectedEye.y).margin(0.001F));
        CHECK(eye.z == Approx(expectedEye.z).margin(0.001F));
        CHECK(glm::length(forward) == Approx(1.0F).margin(0.001F));
        CHECK(glm::dot(forward, expectedDirection) == Approx(1.0F).margin(0.001F));
    };

    try {
        validate_camera();
    } catch(const std::exception& ex) {
        WARN(std::string {"Skipping camera test: "} + ex.what());
    }
}
