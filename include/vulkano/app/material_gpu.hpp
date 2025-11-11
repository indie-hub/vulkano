#pragma once

#include <cstdint>
#include <vector>

#include <glm/vec4.hpp>
#include <glm/gtc/type_precision.hpp>

#include <vulkano/scene/material.hpp>

namespace vulkano::app {
struct MaterialGpu final {
    glm::vec4 baseColorMetallic {1.0F, 1.0F, 1.0F, 0.0F};
    glm::vec4 roughnessAoFlags {0.5F, 1.0F, 0.0F, 0.0F};
    glm::uvec4 textureIndices {0U, 0U, 0U, 0U};
    glm::vec4 textureUsage {0.0F, 0.0F, 0.0F, 0.0F};
    glm::vec4 emissive {0.0F, 0.0F, 0.0F, 0.0F};
};

struct MaterialDescriptorBindings final {
    static constexpr std::uint32_t set {1U};
    static constexpr std::uint32_t materialBufferBinding {0U};
    static constexpr std::uint32_t textureArrayBinding {1U};
    static constexpr std::uint32_t maxTextureSamplers {12U};
};

[[nodiscard]] MaterialGpu make_material_gpu(
    const scene::Material& material, const scene::MaterialTextureHandles& handles) noexcept;
[[nodiscard]] std::vector<MaterialGpu> build_material_gpu_buffer(const scene::MaterialRegistry& registry,
    const std::vector<scene::MaterialTextureHandles>& handles);
}
