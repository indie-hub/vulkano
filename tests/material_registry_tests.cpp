#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vulkano/scene/material.hpp>

#include <glm/vec3.hpp>

#include <stdexcept>

namespace {
constexpr float epsilon {1e-4F};
}

TEST_CASE("Material registry exposes default material") {
    vulkano::scene::MaterialRegistry registry {};

    const vulkano::scene::MaterialId defaultId = registry.default_material_id();
    REQUIRE(defaultId.value == 0U);
    REQUIRE(registry.contains(defaultId));
    REQUIRE(registry.size() == 1U);

    const vulkano::scene::Material& material = registry.material(defaultId);
    REQUIRE(material.textures.baseColorPath.empty());
    using Catch::Matchers::WithinAbs;
    REQUIRE_THAT(material.properties.metallic, WithinAbs(0.0F, epsilon));
    REQUIRE_THAT(material.properties.roughness, WithinAbs(0.5F, epsilon));
    REQUIRE_THAT(material.properties.ambientOcclusion, WithinAbs(1.0F, epsilon));
    REQUIRE_THAT(material.properties.baseColor.x, WithinAbs(0.8F, epsilon));
    REQUIRE_THAT(material.properties.baseColor.y, WithinAbs(0.8F, epsilon));
    REQUIRE_THAT(material.properties.baseColor.z, WithinAbs(0.8F, epsilon));
}

TEST_CASE("Material registry allocates stable ids") {
    vulkano::scene::MaterialRegistry registry {};

    vulkano::scene::Material matteRed {};
    matteRed.properties.baseColor = glm::vec3 {0.9F, 0.1F, 0.1F};
    matteRed.properties.roughness = 0.9F;

    vulkano::scene::Material polishedMetal {};
    polishedMetal.properties.baseColor = glm::vec3 {0.8F, 0.85F, 0.9F};
    polishedMetal.properties.metallic = 1.0F;
    polishedMetal.properties.roughness = 0.1F;

    const vulkano::scene::MaterialId matteId = registry.add_material(matteRed);
    const vulkano::scene::MaterialId metalId = registry.add_material(polishedMetal);

    REQUIRE(matteId.value == 1U);
    REQUIRE(metalId.value == 2U);
    REQUIRE(registry.size() == 3U);

    const vulkano::scene::Material& storedMatte = registry.material(matteId);
    const vulkano::scene::Material& storedMetal = registry.material(metalId);

    using Catch::Matchers::WithinAbs;
    REQUIRE_THAT(storedMatte.properties.baseColor.r, WithinAbs(0.9F, epsilon));
    REQUIRE_THAT(storedMetal.properties.metallic, WithinAbs(1.0F, epsilon));

    vulkano::scene::Material tunedMatte = matteRed;
    tunedMatte.properties.roughness = 0.6F;
    registry.update_material(matteId, tunedMatte);

    REQUIRE_THAT(registry.material(matteId).properties.roughness, WithinAbs(0.6F, epsilon));
}

TEST_CASE("Material registry rejects invalid identifiers") {
    vulkano::scene::MaterialRegistry registry {};

    const vulkano::scene::MaterialId bogusId {42U};
    REQUIRE_FALSE(registry.contains(bogusId));
    REQUIRE_THROWS_AS(registry.material(bogusId), std::out_of_range);
    REQUIRE_THROWS_AS(registry.update_material(bogusId, vulkano::scene::Material {}), std::out_of_range);
}
