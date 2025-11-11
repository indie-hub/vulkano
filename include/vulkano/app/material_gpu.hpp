#pragma once

#include <cstdint>

#include <glm/vec4.hpp>

#include <vulkano/scene/material.hpp>

namespace vulkano::app {
struct MaterialGpu final {
    glm::vec4 baseColorMetallic {1.0F, 1.0F, 1.0F, 0.0F};
    glm::vec4 roughnessAoFlags {0.5F, 1.0F, 0.0F, 0.0F};
};

struct MaterialDescriptorBindings final {
    static constexpr std::uint32_t set {2U};
    static constexpr std::uint32_t materialBufferBinding {0U};
    static constexpr std::uint32_t baseColorTextureBinding {1U};
    static constexpr std::uint32_t normalTextureBinding {2U};
    static constexpr std::uint32_t metallicRoughnessTextureBinding {3U};
    static constexpr std::uint32_t ambientOcclusionTextureBinding {4U};
};

[[nodiscard]] MaterialGpu make_material_gpu(const scene::Material& material) noexcept;
}

