#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace vulkano::app {
struct SurfaceDefaults final {
    static constexpr float metallic {0.0F};
    static constexpr float roughness {0.5F};
    static constexpr float ambientOcclusion {1.0F};
    static constexpr glm::vec4 fallbackSurface {metallic, roughness, ambientOcclusion, 1.0F};
};

} // namespace vulkano::app
