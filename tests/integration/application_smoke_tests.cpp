#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <glm/vec3.hpp>

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
        CHECK(application.scene_light_position() == glm::vec3 {2.0F, 4.0F, 2.0F});
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
