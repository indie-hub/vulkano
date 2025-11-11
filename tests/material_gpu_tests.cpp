#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vulkano/app/material_gpu.hpp>

#include <glm/vec3.hpp>

namespace {
constexpr float epsilon {1e-4F};
}

TEST_CASE("Material GPU conversion clamps values") {
    vulkano::scene::Material material {};
    material.properties.baseColor = glm::vec3 {1.5F, -0.2F, 0.25F};
    material.properties.metallic = 2.0F;
    material.properties.roughness = -1.0F;
    material.properties.ambientOcclusion = 4.0F;
    material.useBaseColorTexture = true;
    material.useNormalTexture = false;
    material.useMetallicRoughnessTexture = true;
    material.useAmbientOcclusionTexture = true;
    material.properties.emissive = glm::vec3 {0.2F, 0.3F, 0.4F};
    material.properties.emissiveIntensity = 1.5F;
    material.properties.normalStrength = 2.0F;

    vulkano::scene::MaterialTextureHandles handles {};
    handles.baseColor = 5U;
    handles.normal = 6U;
    handles.metallicRoughness = 7U;
    handles.ambientOcclusion = 8U;

    const vulkano::app::MaterialGpu gpu = vulkano::app::make_material_gpu(material, handles);

    using Catch::Matchers::WithinAbs;
    REQUIRE_THAT(gpu.baseColorMetallic.x, WithinAbs(1.5F, epsilon));
    REQUIRE_THAT(gpu.baseColorMetallic.y, WithinAbs(-0.2F, epsilon));
    REQUIRE_THAT(gpu.baseColorMetallic.z, WithinAbs(0.25F, epsilon));
    REQUIRE_THAT(gpu.baseColorMetallic.w, WithinAbs(1.0F, epsilon));
    REQUIRE_THAT(gpu.roughnessAoStrength.x, WithinAbs(0.0F, epsilon));
    REQUIRE_THAT(gpu.roughnessAoStrength.y, WithinAbs(1.0F, epsilon));
    REQUIRE_THAT(gpu.roughnessAoStrength.z, WithinAbs(1.0F, epsilon));
    REQUIRE(gpu.textureIndices.x == 5U);
    REQUIRE(gpu.textureIndices.y == 6U);
    REQUIRE(gpu.textureIndices.z == 7U);
    REQUIRE(gpu.textureIndices.w == 8U);
    using Catch::Matchers::WithinAbs;
    REQUIRE_THAT(gpu.textureUsage.x, WithinAbs(1.0F, epsilon));
    REQUIRE_THAT(gpu.textureUsage.y, WithinAbs(0.0F, epsilon));
    REQUIRE_THAT(gpu.textureUsage.z, WithinAbs(1.0F, epsilon));
    REQUIRE_THAT(gpu.textureUsage.w, WithinAbs(1.0F, epsilon));
    REQUIRE_THAT(gpu.emissive.x, WithinAbs(0.2F, epsilon));
    REQUIRE_THAT(gpu.emissive.y, WithinAbs(0.3F, epsilon));
    REQUIRE_THAT(gpu.emissive.z, WithinAbs(0.4F, epsilon));
    REQUIRE_THAT(gpu.emissive.w, WithinAbs(1.5F, epsilon));
}

TEST_CASE("Material descriptor bindings expose layout constants") {
    REQUIRE(vulkano::app::MaterialDescriptorBindings::materialBufferBinding == 0U);
    REQUIRE(vulkano::app::MaterialDescriptorBindings::textureArrayBinding == 1U);
    REQUIRE(vulkano::app::MaterialDescriptorBindings::maxTextureSamplers == 12U);
}

TEST_CASE("Material GPU buffer builder always returns at least one entry") {
    vulkano::scene::MaterialRegistry registry {};
    std::vector<vulkano::scene::MaterialTextureHandles> handles {vulkano::scene::MaterialTextureHandles {}};
    auto buffer = vulkano::app::build_material_gpu_buffer(registry, handles);
    REQUIRE(buffer.size() == 1U);

    vulkano::scene::Material extra {};
    extra.properties.baseColor = glm::vec3 {0.25F, 0.5F, 0.75F};
    extra.useMetallicRoughnessTexture = true;
    const auto newId = registry.add_material(extra);

    handles.push_back(vulkano::scene::MaterialTextureHandles {.baseColor = 3U, .normal = 4U, .metallicRoughness = 5U, .ambientOcclusion = 6U});
    buffer = vulkano::app::build_material_gpu_buffer(registry, handles);
    REQUIRE(buffer.size() == 2U);
    using Catch::Matchers::WithinAbs;
    REQUIRE_THAT(buffer[1].baseColorMetallic.x, WithinAbs(0.25F, epsilon));
    REQUIRE(buffer[1].textureIndices.x == 3U);
    REQUIRE_THAT(buffer[1].textureUsage.z, WithinAbs(1.0F, epsilon));
    REQUIRE(newId.value == 1U);
}
