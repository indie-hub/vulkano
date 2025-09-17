#include <vulkano/config.hpp>

namespace vulkano::config {

const std::uint32_t AlbedoSize {256U};
const std::uint32_t AlbedoSquares {8U};
const glm::vec3 AlbedoLight {0.92F, 0.92F, 0.92F};
const glm::vec3 AlbedoDark {0.12F, 0.12F, 0.12F};

const std::uint32_t NormalSize {128U};
const float NormalAmplitude {0.8F};

} // namespace vulkano::config

