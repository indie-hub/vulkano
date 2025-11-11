#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace vulkano::app {
struct GpuVertex final {
    glm::vec3 position {};
    glm::vec3 normal {};
    glm::vec3 color {};
    glm::vec2 uv {};
    glm::vec3 tangent {};
    float bitangentSign {1.0F};
};
} // namespace vulkano::app
