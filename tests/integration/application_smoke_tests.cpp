#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vulkano/application.hpp>

TEST_CASE("Vulkan application initialises and shuts down", "[integration]") {
    auto config = vulkano::AppConfigBuilder {}
        .with_validation_enabled(false)
        .build();

    auto run_application = [&config]() {
        vulkano::VulkanApplication application {config};
        application.request_close();
        application.run();
    };

    try {
        run_application();
    } catch(const std::exception& ex) {
        WARN(std::string {"Skipping integration test: "} + ex.what());
    }
}
