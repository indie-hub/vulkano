#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vulkano/app/material_gpu.hpp>

namespace {
constexpr float epsilon {1e-4F};
}

TEST_CASE("Material GPU conversion clamps values") {
    vulkano::scene::Material material {};
    material.properties.baseColor = glm::vec3 {1.5F, -0.2F, 0.25F};
    material.properties.metallic = 2.0F;
    material.properties.roughness = -1.0F;
    material.properties.ambientOcclusion = 4.0F;

    const vulkano::app::MaterialGpu gpu = vulkano::app::make_material_gpu(material);

    using Catch::Matchers::WithinAbs;
    REQUIRE_THAT(gpu.baseColorMetallic.x, WithinAbs(1.5F, epsilon));
    REQUIRE_THAT(gpu.baseColorMetallic.y, WithinAbs(-0.2F, epsilon));
    REQUIRE_THAT(gpu.baseColorMetallic.z, WithinAbs(0.25F, epsilon));
    REQUIRE_THAT(gpu.baseColorMetallic.w, WithinAbs(1.0F, epsilon));
    REQUIRE_THAT(gpu.roughnessAoFlags.x, WithinAbs(0.0F, epsilon));
    REQUIRE_THAT(gpu.roughnessAoFlags.y, WithinAbs(1.0F, epsilon));
}

TEST_CASE("Material descriptor bindings expose layout constants") {
    REQUIRE(vulkano::app::MaterialDescriptorBindings::materialBufferBinding == 0U);
    REQUIRE(vulkano::app::MaterialDescriptorBindings::baseColorTextureBinding == 1U);
    REQUIRE(vulkano::app::MaterialDescriptorBindings::normalTextureBinding == 2U);
    REQUIRE(vulkano::app::MaterialDescriptorBindings::metallicRoughnessTextureBinding == 3U);
    REQUIRE(vulkano::app::MaterialDescriptorBindings::ambientOcclusionTextureBinding == 4U);
}

