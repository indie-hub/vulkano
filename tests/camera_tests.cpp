// Unit tests for FPS-style camera operations
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <glm/vec3.hpp>
#include <vulkano/camera.hpp>

TEST_CASE("camera_look_delta_keeps_eye_position") {
    vulkano::Camera cam;
    const glm::vec3 eye0 {cam.position()};
    cam.look_delta(0.1F, 0.05F);
    const glm::vec3 eye1 {cam.position()};
    REQUIRE(eye1.x == Catch::Approx(eye0.x).margin(1e-5));
    REQUIRE(eye1.y == Catch::Approx(eye0.y).margin(1e-5));
    REQUIRE(eye1.z == Catch::Approx(eye0.z).margin(1e-5));
}

TEST_CASE("camera_move_local_changes_position") {
    vulkano::Camera cam;
    const glm::vec3 eye0 {cam.position()};
    cam.move_local(glm::vec3 {0.0F, 0.0F, 1.0F}); // move forward 1 unit
    const glm::vec3 eye1 {cam.position()};
    // Expect position to change by some amount; exact direction depends on initial yaw/pitch
    const float dx {eye1.x - eye0.x};
    const float dy {eye1.y - eye0.y};
    const float dz {eye1.z - eye0.z};
    const float distSq {dx*dx + dy*dy + dz*dz};
    REQUIRE(distSq == Catch::Approx(1.0F).margin(1e-3));
}

TEST_CASE("camera_forward_and_right_are_orthogonal") {
    vulkano::Camera cam;
    const glm::vec3 f {cam.forward()};
    const glm::vec3 r {cam.right()};
    const float dotFR {f.x * r.x + f.y * r.y + f.z * r.z};
    REQUIRE(dotFR == Catch::Approx(0.0F).margin(1e-4));
}

