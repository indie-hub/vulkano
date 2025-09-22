#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vulkano/app_config.hpp>

TEST_CASE("AppConfigBuilder provides sensible defaults", "[config]") {
    const auto config = vulkano::AppConfigBuilder {}.build();

    CHECK(config.application_name() == "Vulkano Codex");
    CHECK(config.engine_name() == "Codex Engine");
    CHECK(config.window_extent().width == 1280U);
    CHECK(config.window_extent().height == 720U);
    CHECK(config.validation_enabled());
}

TEST_CASE("AppConfigBuilder accepts overrides", "[config]") {
    constexpr vulkano::WindowExtent extent {800U, 600U};
    const auto config = vulkano::AppConfigBuilder {}
        .with_application_name("Unit Test App")
        .with_engine_name("Unit Test Engine")
        .with_window_extent(extent)
        .with_validation_enabled(false)
        .build();

    CHECK(config.application_name() == "Unit Test App");
    CHECK(config.engine_name() == "Unit Test Engine");
    CHECK(config.window_extent().width == extent.width);
    CHECK(config.window_extent().height == extent.height);
    CHECK_FALSE(config.validation_enabled());
}
