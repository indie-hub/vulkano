#include <vulkano/config.hpp>

namespace vulkano::config {

const std::uint32_t AlbedoSize {256U};
const std::uint32_t AlbedoSquares {8U};
const glm::vec3 AlbedoLight {0.92F, 0.92F, 0.92F};
const glm::vec3 AlbedoDark {0.12F, 0.12F, 0.12F};

const std::uint32_t NormalSize {128U};
const float NormalAmplitude {0.8F};

// SSAO defaults
const bool SsaoEnabledByDefault {true};
const std::uint32_t SsaoDefaultKernelSize {32U};
const float SsaoDefaultRadius {0.5F};
const float SsaoDefaultBias {0.02F};
const float SsaoDefaultPower {1.0F};
const bool SsaoBlurEnabledByDefault {true};
const std::uint32_t SsaoDefaultBlurRadius {2U};
const float SsaoDefaultBlurSigma {1.2F};
const float SsaoDefaultStrength {1.0F};
const std::uint32_t SsaoNoiseTextureSize {16U};

} // namespace vulkano::config
