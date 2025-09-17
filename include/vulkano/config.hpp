#pragma once

#include <cstdint>

#include <glm/vec3.hpp>

namespace vulkano::config {

// Procedural texture configuration (centralized to avoid magic numbers)
extern const std::uint32_t AlbedoSize;
extern const std::uint32_t AlbedoSquares;
extern const glm::vec3 AlbedoLight;
extern const glm::vec3 AlbedoDark;

extern const std::uint32_t NormalSize;
extern const float NormalAmplitude;

// SSAO defaults (centralized to avoid magic numbers)
extern const bool SsaoEnabledByDefault;
extern const std::uint32_t SsaoDefaultKernelSize; // one of {16, 32, 64}
extern const float SsaoDefaultRadius;             // e.g., 0.5
extern const float SsaoDefaultBias;               // e.g., 0.02
extern const float SsaoDefaultPower;              // e.g., 1.0
extern const bool SsaoBlurEnabledByDefault;       // enable blur pass
extern const std::uint32_t SsaoDefaultBlurRadius; // [1..5]
extern const float SsaoDefaultBlurSigma;          // e.g., 1.0
extern const float SsaoDefaultStrength;           // AO strength multiplier
extern const std::uint32_t SsaoNoiseTextureSize;  // NxN tiling noise texture size

} // namespace vulkano::config
