#include <vulkano/app/material_gpu.hpp>

#include <vector>

#include <glm/common.hpp>

namespace vulkano::app {
namespace {
[[nodiscard]] float clamp01(float value) noexcept {
    return glm::clamp(value, 0.0F, 1.0F);
}
}

MaterialGpu make_material_gpu(const scene::Material& material, const scene::MaterialTextureHandles& handles) noexcept {
    MaterialGpu gpu {};
    const scene::MaterialProperties& props = material.properties;

    const float metallic = clamp01(props.metallic);
    const float roughness = clamp01(props.roughness);
    const float ambient = clamp01(props.ambientOcclusion);

    gpu.baseColorMetallic = glm::vec4 {props.baseColor, metallic};
    gpu.roughnessAoFlags = glm::vec4 {roughness, ambient, 0.0F, 0.0F};
    gpu.textureIndices = glm::uvec4 {
        handles.baseColor,
        handles.normal,
        handles.metallicRoughness,
        handles.ambientOcclusion
    };
    gpu.textureUsage = glm::vec4 {
        material.useBaseColorTexture ? 1.0F : 0.0F,
        material.useNormalTexture ? 1.0F : 0.0F,
        material.useMetallicRoughnessTexture ? 1.0F : 0.0F,
        material.useAmbientOcclusionTexture ? 1.0F : 0.0F
    };

    return gpu;
}

std::vector<MaterialGpu> build_material_gpu_buffer(const scene::MaterialRegistry& registry,
    const std::vector<scene::MaterialTextureHandles>& handles) {
    const std::vector<scene::Material>& materials = registry.materials();
    std::vector<MaterialGpu> buffer;
    buffer.reserve(materials.size());
    for (std::size_t i {0U}; i < materials.size(); ++i) {
        const scene::Material& material = materials[i];
        const scene::MaterialTextureHandles& handle = i < handles.size() ? handles[i] : scene::MaterialTextureHandles {};
        buffer.push_back(make_material_gpu(material, handle));
    }
    if (buffer.empty()) {
        buffer.push_back(MaterialGpu {});
    }
    return buffer;
}
} // namespace vulkano::app
