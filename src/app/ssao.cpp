#include <vulkano/app/ssao.hpp>

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

namespace vulkano::app {
namespace {
[[nodiscard]] float lerp(float a, float b, float t) noexcept {
    return std::lerp(a, b, t);
}
}

SSAOSampleGenerator::SSAOSampleGenerator(std::uint32_t seed) noexcept
    : m_seed {seed} {
}

std::mt19937 SSAOSampleGenerator::create_engine(std::uint32_t offset) const noexcept {
    return std::mt19937 {m_seed + offset};
}

std::vector<glm::vec3> SSAOSampleGenerator::generate_kernel(std::size_t sampleCount) const {
    if (sampleCount == 0U) {
        return {};
    }

    std::vector<glm::vec3> kernel;
    kernel.reserve(sampleCount);

    std::mt19937 engine = create_engine();
    std::uniform_real_distribution<float> randomFloats {0.0F, 1.0F};

    for (std::size_t i {0U}; i < sampleCount; ++i) {
        glm::vec3 sample {
            randomFloats(engine) * 2.0F - 1.0F,
            randomFloats(engine) * 2.0F - 1.0F,
            randomFloats(engine)
        };
        sample = glm::normalize(sample);

        const float progress = static_cast<float>(i) / static_cast<float>(sampleCount);
        const float scale = lerp(0.1F, 1.0F, progress * progress);
        sample *= scale;
        kernel.push_back(sample);
    }

    return kernel;
}

std::vector<glm::vec3> SSAOSampleGenerator::generate_noise(std::size_t sampleCount) const {
    if (sampleCount == 0U) {
        return {};
    }

    std::vector<glm::vec3> noise;
    noise.reserve(sampleCount);

    std::mt19937 engine = create_engine(1U);
    std::uniform_real_distribution<float> randomFloats {0.0F, 1.0F};

    for (std::size_t i {0U}; i < sampleCount; ++i) {
        glm::vec3 vector {
            randomFloats(engine) * 2.0F - 1.0F,
            randomFloats(engine) * 2.0F - 1.0F,
            0.0F
        };

        if (glm::length(vector) > 0.0F) {
            vector = glm::normalize(vector);
        } else {
            vector = glm::vec3 {1.0F, 0.0F, 0.0F};
        }

        vector *= randomFloats(engine);
        noise.push_back(vector);
    }

    return noise;
}
} // namespace vulkano::app
