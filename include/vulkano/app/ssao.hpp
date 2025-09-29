#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace vulkano::app {
class SSAOSampleGenerator final {
public:
    explicit SSAOSampleGenerator(std::uint32_t seed = default_seed) noexcept;

    [[nodiscard]] std::vector<glm::vec3> generate_kernel(std::size_t sampleCount) const;
    [[nodiscard]] std::vector<glm::vec3> generate_noise(std::size_t sampleCount) const;

    static constexpr std::uint32_t default_seed {1337U};

private:
    [[nodiscard]] std::mt19937 create_engine(std::uint32_t offset = 0U) const noexcept;

    std::uint32_t m_seed {default_seed};
};
}
