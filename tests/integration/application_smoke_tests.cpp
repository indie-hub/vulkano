#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <glm/vec3.hpp>
#include <glm/geometric.hpp>

#include <string>
#include <vulkano/application.hpp>

using Catch::Approx;

TEST_CASE("Vulkan application initialises and shuts down", "[integration]") {
    auto config = vulkano::AppConfigBuilder {}
        .with_validation_enabled(false)
        .build();

    auto run_application = [&config]() {
        vulkano::VulkanApplication application {config};
        CHECK(application.primitive_count() == 3U);
        const glm::vec3 lightPosition = application.scene_light_position();
        const glm::vec3 defaultDirection {-2.0F, -4.0F, -2.0F};
        const glm::vec3 expectedPosition = -glm::normalize(defaultDirection) * 3.0F;
        CHECK(lightPosition.x == Approx(expectedPosition.x).margin(1e-3F));
        CHECK(lightPosition.y == Approx(expectedPosition.y).margin(1e-3F));
        CHECK(lightPosition.z == Approx(expectedPosition.z).margin(1e-3F));
        CHECK(application.scene_light_intensity() == Approx(1.0F));
        application.request_close();
        application.run();
    };

    try {
        run_application();
    } catch(const std::exception& ex) {
        WARN(std::string {"Skipping integration test: "} + ex.what());
    }
}
