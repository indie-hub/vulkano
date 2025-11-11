#include <vulkano/app/light_gpu.hpp>

#include <glm/geometric.hpp>

namespace vulkano::app {
namespace {
[[nodiscard]] glm::vec3 normalize_or_default(const glm::vec3& direction) noexcept {
    const float length = glm::length(direction);
    if (length > 0.0F) {
        return direction / length;
    }
    return glm::vec3 {0.0F, -1.0F, 0.0F};
}
} // namespace

std::vector<LightGpu> build_light_gpu_buffer(const scene::LightRegistry& registry) {
    std::vector<LightGpu> buffer;
    buffer.reserve(registry.size());
    for (const scene::Light& light : registry.lights()) {
        LightGpu gpu {};
        const glm::vec3 direction = normalize_or_default(light.direction);
        gpu.directionIntensity = glm::vec4 {direction, light.intensity};
        gpu.colorType = glm::vec4 {light.color, static_cast<float>(light.type)};
        buffer.push_back(gpu);
    }
    if (buffer.empty()) {
        buffer.push_back(LightGpu {});
    }
    return buffer;
}
} // namespace vulkano::app

