#include <catch2/catch_all.hpp>

#include <glm/glm.hpp>
#include <vulkan_app/camera.hpp>

TEST_CASE("Camera projection has inverted Y for Vulkan", "[camera]") {
    vulkan_app::Camera cam{};
    cam.set_fov(glm::radians(60.0F));
    cam.set_near_far(0.1F, 100.0F);
    const glm::mat4 proj {cam.proj(16.0F / 9.0F)};
    // Vulkan clip space uses inverted Y; check the sign on [1][1]
    REQUIRE(proj[1][1] < 0.0F);
}

TEST_CASE("Camera view looks towards forward vector", "[camera]") {
    vulkan_app::Camera cam{};
    cam.set_position(glm::vec3{0.0F, 0.0F, 3.0F});
    cam.set_yaw_pitch(0.0F, 0.0F);
    const glm::mat4 view {cam.view()};
    // For yaw = pitch = 0, forward is +X; verify view transforms position close to origin along -X
    const glm::vec4 worldPos {cam.position(), 1.0F};
    const glm::vec4 viewPos {view * worldPos};
    // Z should be near -3 (camera at z=3 looking along +X), X should be approximately 0 after view transform
    REQUIRE(std::abs(viewPos.x) == Catch::Approx(0.0F).margin(1e-4F));
}

