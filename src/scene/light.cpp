#include <vulkano/scene/light.hpp>

#include <stdexcept>

#include <glm/geometric.hpp>

namespace vulkano::scene {
namespace {
[[nodiscard]] glm::vec3 normalize_or_default(const glm::vec3& direction) noexcept {
    const float length = glm::length(direction);
    if (length > 0.0F) {
        return direction / length;
    }
    return glm::vec3 {0.0F, -1.0F, 0.0F};
}
} // namespace

LightRegistry::LightRegistry() {
    Light defaultLight {};
    defaultLight.direction = glm::vec3 {0.0F, -1.0F, 0.0F};
    defaultLight.color = glm::vec3 {1.0F, 1.0F, 1.0F};
    defaultLight.intensity = 1.0F;
    m_lights.push_back(defaultLight);
}

LightId LightRegistry::add_light(const Light& light) {
    Light normalized = light;
    normalized.direction = normalize_or_default(light.direction);
    m_lights.push_back(normalized);
    return LightId {static_cast<std::uint32_t>(m_lights.size() - 1U)};
}

void LightRegistry::update_light(LightId id, const Light& light) {
    if (!is_valid(id)) {
        throw std::out_of_range {"LightRegistry::update_light received invalid light id"};
    }
    Light normalized = light;
    normalized.direction = normalize_or_default(light.direction);
    m_lights[id.value] = normalized;
}

const Light& LightRegistry::light(LightId id) const {
    if (!is_valid(id)) {
        throw std::out_of_range {"LightRegistry::light received invalid light id"};
    }
    return m_lights[id.value];
}

Light& LightRegistry::light(LightId id) {
    if (!is_valid(id)) {
        throw std::out_of_range {"LightRegistry::light received invalid light id"};
    }
    return m_lights[id.value];
}

std::size_t LightRegistry::size() const noexcept {
    return m_lights.size();
}

const std::vector<Light>& LightRegistry::lights() const noexcept {
    return m_lights;
}

bool LightRegistry::is_valid(LightId id) const noexcept {
    return id.value < m_lights.size();
}
} // namespace vulkano::scene

