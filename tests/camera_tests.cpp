#include <catch2/catch_test_macros.hpp>

#include <vulkano/app/camera.hpp>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/vec3.hpp>

namespace {
constexpr float EPS {1e-4F};
}

TEST_CASE("Camera default orientation looks down -Z") {
    vulkano::app::Camera camera {};
    const glm::vec3 forward = camera.forward();
    REQUIRE(glm::all(glm::epsilonEqual(forward, glm::vec3 {0.0F, 0.0F, -1.0F}, EPS)));
}

TEST_CASE("Camera yaw rotation to the right") {
    vulkano::app::Camera camera {};
    camera.rotate(90.0F, 0.0F, 1.0F);
    const glm::vec3 forward = camera.forward();
    REQUIRE(glm::all(glm::epsilonEqual(glm::vec3 {1.0F, 0.0F, 0.0F}, forward, EPS)));
}

TEST_CASE("Camera pitch clamps to avoid gimbal lock") {
    vulkano::app::Camera camera {};
    camera.rotate(0.0F, 100.0F, 1.0F);
    const glm::vec3 forward = camera.forward();
    REQUIRE(forward.y < 1.0F);
    REQUIRE(forward.y > 0.0F);
}

TEST_CASE("Camera movement responds to directions") {
    vulkano::app::Camera camera {};
    const glm::vec3 initial = camera.position();
    camera.move(vulkano::app::Camera::Movement::Forward, 1.0F, 2.0F);
    REQUIRE(camera.position().z < initial.z);
    camera.move(vulkano::app::Camera::Movement::Up, 0.5F, 2.0F);
    REQUIRE(camera.position().y > initial.y);
}
