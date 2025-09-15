#pragma once

#include <array>
#include <cstdint>

#include <glm/glm.hpp>

namespace vulkan_app {

/**
 * @brief Uniform buffer for camera matrices and position.
 */
struct CameraUBO final {
    glm::mat4 view{};
    glm::mat4 proj{};
    glm::mat4 viewProj{};
    glm::vec3 cameraPos{};
    float pad0{0.0F};
};

/**
 * @brief Point/spot light parameters for Phong lighting.
 */
struct Light final {
    glm::vec3 position{0.0F, 2.0F, 2.0F};
    float intensity{3.0F};
    glm::vec3 color{1.0F, 1.0F, 1.0F};
    float shininess{64.0F};
};

/**
 * @brief Material parameters for simple Phong + normal mapping.
 */
struct Material final {
    glm::vec3 albedo{1.0F, 1.0F, 1.0F};
    float normalStrength{1.0F};
};

/**
 * @brief Parameters controlling SSAO kernel and post-filtering.
 */
struct SsaoParams final {
    int kernelSize{32};
    float radius{0.5F};
    float bias{0.025F};
    float power{1.0F};
    int blurRadius{2};
    int enableBlur{1};
    int enabled{1};
    int pad{0};
};

} // namespace vulkan_app
