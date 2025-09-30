#include <vulkano/app/material_gpu.hpp>

#include <vector>

#include <glm/common.hpp>

namespace vulkano::app {
namespace {
[[nodiscard]] float clamp01(float value) noexcept {
    return glm::clamp(value, 0.0F, 1.0F);
}
}

MaterialGpu make_material_gpu(const scene::Material& material) noexcept {
    MaterialGpu gpu {};
    const scene::MaterialProperties& props = material.properties;

    const float metallic = clamp01(props.metallic);
    const float roughness = clamp01(props.roughness);
    const float ambient = clamp01(props.ambientOcclusion);

    gpu.baseColorMetallic = glm::vec4 {props.baseColor, metallic};
    gpu.roughnessAoFlags = glm::vec4 {roughness, ambient, 0.0F, 0.0F};

    return gpu;
}

std::vector<MaterialGpu> build_material_gpu_buffer(const scene::MaterialRegistry& registry) {
    const std::vector<scene::Material>& materials = registry.materials();
    std::vector<MaterialGpu> buffer;
    buffer.reserve(materials.size());
    for (const scene::Material& material : materials) {
        buffer.push_back(make_material_gpu(material));
    }
    if (buffer.empty()) {
        buffer.push_back(MaterialGpu {});
    }
    return buffer;
}
} // namespace vulkano::app
