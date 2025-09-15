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
    glm::vec3 position{0.0F, 2.0F, 2.0F}; ///< World-space position
    float intensity{3.0F};                ///< Scalar intensity multiplier
    glm::vec3 color{1.0F, 1.0F, 1.0F};    ///< RGB color (linear)
    float shininess{64.0F};               ///< Blinn-Phong shininess exponent
};

/**
 * @brief Material parameters for simple Phong + normal mapping.
 */
struct Material final {
    glm::vec3 albedo{1.0F, 1.0F, 1.0F}; ///< Base color (linear); sampled from sRGB texture
    float normalStrength{1.0F};         ///< Scale for tangent-space normal perturbation
};

/**
 * @brief Parameters controlling SSAO kernel and post-filtering.
 */
struct SsaoParams final {
    int kernelSize{32};  ///< Number of hemisphere samples (e.g., 16/32/64)
    float radius{0.5F};  ///< Sample radius in view-space units
    float bias{0.025F};  ///< Bias to reduce self-occlusion
    float power{1.0F};   ///< AO exponent for contrast
    int blurRadius{2};   ///< Radius for separable Gaussian blur (if enabled)
    int enableBlur{1};   ///< Toggle AO blur pass
    int enabled{1};      ///< Toggle SSAO effect
    int pad{0};          ///< Padding for std140 alignment
};

} // namespace vulkan_app
