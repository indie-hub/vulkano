#pragma once

#include <vector>

#include <glm/vec4.hpp>

#include <vulkano/scene/light.hpp>

namespace vulkano::app {
struct LightGpu final {
    glm::vec4 directionIntensity {0.0F, -1.0F, 0.0F, 1.0F};
    glm::vec4 colorType {1.0F, 1.0F, 1.0F, 0.0F};
    glm::vec4 positionRange {0.0F, 2.0F, 0.0F, 10.0F};
    glm::vec4 shadowParams {0.0F, 0.0F, 0.0F, 0.0F};
};

[[nodiscard]] std::vector<LightGpu> build_light_gpu_buffer(const scene::LightRegistry& registry);
}
