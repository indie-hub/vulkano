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

} // namespace vulkano::config

