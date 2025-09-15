#include <catch2/catch_all.hpp>

#include <vulkan_app/icosphere.hpp>

TEST_CASE("Icosphere vertex and index counts", "[icosphere]") {
    const auto mesh0 {vulkan_app::make_icosphere(0)};
    REQUIRE(mesh0.vertices.size() == 12);
    REQUIRE(mesh0.indices.size() == 60);

    const auto mesh1 {vulkan_app::make_icosphere(1)};
    REQUIRE(mesh1.vertices.size() == 42);
    REQUIRE(mesh1.indices.size() == 240);
}
