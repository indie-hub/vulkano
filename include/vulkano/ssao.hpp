#pragma once

#include <cstdint>
#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace vulkano {

// Generate a hemisphere-distributed SSAO kernel of the given size.
// Samples are in +Z hemisphere, normalized, and scaled to concentrate near origin.
std::vector<glm::vec3> generate_ssao_kernel(std::uint32_t size) noexcept;

// Generate N 2D noise vectors in [-1,1]^2 normalized; useful for CPU validation.
std::vector<glm::vec2> generate_ssao_noise_vectors(std::uint32_t count, std::uint32_t seed = 424242U) noexcept;

} // namespace vulkano

