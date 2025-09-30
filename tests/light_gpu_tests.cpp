#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vulkano/app/light_gpu.hpp>

TEST_CASE("Light GPU builder normalizes and packs data") {
    using Catch::Matchers::WithinAbs;

    vulkano::scene::LightRegistry registry {};

    vulkano::scene::Light light {};
    light.direction = glm::vec3 {0.0F, -2.0F, 0.0F};
    light.color = glm::vec3 {0.5F, 0.8F, 0.3F};
    light.intensity = 2.0F;
    const auto id = registry.add_light(light);
    REQUIRE(id.value == 1U);

    const auto buffer = vulkano::app::build_light_gpu_buffer(registry);
    REQUIRE(buffer.size() == 2U);

    const vulkano::app::LightGpu& gpu = buffer[1];
    REQUIRE_THAT(gpu.directionIntensity.x, WithinAbs(0.0F, 1e-4F));
    REQUIRE_THAT(gpu.directionIntensity.y, WithinAbs(-1.0F, 1e-4F));
    REQUIRE_THAT(gpu.directionIntensity.w, WithinAbs(2.0F, 1e-4F));
    REQUIRE_THAT(gpu.colorType.x, WithinAbs(0.5F, 1e-4F));
    REQUIRE_THAT(gpu.colorType.w, WithinAbs(static_cast<float>(vulkano::scene::LightType::Directional), 1e-4F));
}
