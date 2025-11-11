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

std::vector<LightGpu> build_light_gpu_buffer(const scene::LightRegistry& registry,
    const std::unordered_map<std::uint32_t, std::uint32_t>& shadowIndices) {
    std::vector<LightGpu> buffer;
    buffer.reserve(registry.size());
    for (std::size_t idx {0U}; idx < registry.size(); ++idx) {
        const scene::Light& light = registry.light(scene::LightId {static_cast<std::uint32_t>(idx)});
        LightGpu gpu {};
        glm::vec3 direction = light.direction;
        if (light.type == scene::LightType::Directional) {
            direction = normalize_or_default(light.direction);
        }
        gpu.directionIntensity = glm::vec4 {direction, light.intensity};
        const float typeValue = light.type == scene::LightType::Directional ? 0.0F : 1.0F;
        const float castsShadowValue = light.castsShadow ? 1.0F : 0.0F;
        gpu.colorType = glm::vec4 {light.color, typeValue};
        float shadowIndex = -1.0F;
        const auto shadowIt = shadowIndices.find(static_cast<std::uint32_t>(idx));
        if (shadowIt != shadowIndices.end()) {
            shadowIndex = static_cast<float>(shadowIt->second);
        }
        gpu.shadowParams = glm::vec4 {castsShadowValue, shadowIndex, 0.0F, 0.0F};
        gpu.positionRange = glm::vec4 {light.position, light.range};
        buffer.push_back(gpu);
    }
    if (buffer.empty()) {
        buffer.push_back(LightGpu {});
    }
    return buffer;
}
} // namespace vulkano::app
