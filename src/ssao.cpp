#include <vulkano/ssao.hpp>

#include <random>
#include <algorithm>
#include <glm/geometric.hpp>

namespace vulkano {

std::vector<glm::vec3> generate_ssao_kernel(std::uint32_t size) noexcept {
    std::vector<glm::vec3> kernel;
    if (size == 0U) {
        return kernel;
    }
    kernel.reserve(static_cast<std::size_t>(size));
    std::mt19937 rng(1337U);
    std::uniform_real_distribution<float> uni(0.0F, 1.0F);
    for (std::uint32_t i {0U}; i < size; ++i) {
        float x = uni(rng) * 2.0F - 1.0F;
        float y = uni(rng) * 2.0F - 1.0F;
        float z = uni(rng);
        glm::vec3 s {x, y, z};
        if (glm::length(s) > 0.0F) {
            s = glm::normalize(s);
        }
        float t = static_cast<float>(i) / static_cast<float>(size);
        float scale = 0.1F + (1.0F - 0.1F) * (t * t);
        s *= scale;
        kernel.push_back(s);
    }
    return kernel;
}

std::vector<glm::vec2> generate_ssao_noise_vectors(std::uint32_t count, std::uint32_t seed) noexcept {
    std::vector<glm::vec2> v;
    if (count == 0U) {
        return v;
    }
    v.reserve(static_cast<std::size_t>(count));
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> u(-1.0F, 1.0F);
    for (std::uint32_t i {0U}; i < count; ++i) {
        glm::vec2 p {u(rng), u(rng)};
        if (glm::length(p) > 0.0F) {
            p = glm::normalize(p);
        }
        v.push_back(p);
    }
    return v;
}

} // namespace vulkano

