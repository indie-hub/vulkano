#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vulkano/scene/light.hpp>

#include <glm/vec3.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/geometric.hpp>

TEST_CASE("Light registry has default directional light") {
    using Catch::Matchers::WithinAbs;

    vulkano::scene::LightRegistry registry {};

    REQUIRE(registry.size() == 1U);
    const vulkano::scene::Light& light = registry.light(vulkano::scene::LightId {0U});
    REQUIRE(light.type == vulkano::scene::LightType::Directional);
    const glm::vec3 expectedDirection = glm::normalize(glm::vec3 {-0.5F, -1.0F, -0.25F});
    REQUIRE(glm::all(glm::epsilonEqual(light.direction, expectedDirection, 1e-4F)));
    REQUIRE_THAT(light.intensity, WithinAbs(2.5F, 1e-4F));
}

TEST_CASE("Light registry normalizes direction") {
    using Catch::Matchers::WithinAbs;

    vulkano::scene::LightRegistry registry {};

    vulkano::scene::Light light {};
    light.direction = glm::vec3 {2.0F, 0.0F, 0.0F};
    light.intensity = 2.5F;
    const vulkano::scene::LightId id = registry.add_light(light);

    const vulkano::scene::Light& stored = registry.light(id);
    REQUIRE(stored.direction == glm::vec3 {1.0F, 0.0F, 0.0F});
    REQUIRE_THAT(stored.intensity, WithinAbs(2.5F, 1e-4F));
}

TEST_CASE("Light registry updates existing entries") {
    using Catch::Matchers::WithinAbs;

    vulkano::scene::LightRegistry registry {};

    vulkano::scene::Light light {};
    light.direction = glm::vec3 {0.0F, 0.0F, 1.0F};
    light.intensity = 0.5F;
    const vulkano::scene::LightId id = registry.add_light(light);

    vulkano::scene::Light updated = light;
    updated.direction = glm::vec3 {0.0F, -2.0F, 0.0F};
    updated.intensity = 3.0F;
    registry.update_light(id, updated);

    const vulkano::scene::Light& stored = registry.light(id);
    REQUIRE(stored.direction == glm::vec3 {0.0F, -1.0F, 0.0F});
    REQUIRE_THAT(stored.intensity, WithinAbs(3.0F, 1e-4F));
}
