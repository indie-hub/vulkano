#include <vulkano/scene/light.hpp>

#include <stdexcept>
#include <cstddef>

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

[[nodiscard]] float sanitize_range(float range) noexcept {
    return range <= 0.0F ? 0.01F : range;
}

[[nodiscard]] Light sanitize_light(const Light& light) noexcept {
    Light sanitized = light;
    if (sanitized.type == LightType::Directional) {
        sanitized.direction = normalize_or_default(light.direction);
    }
    sanitized.range = sanitize_range(light.range);
    return sanitized;
}
} // namespace

LightRegistry::LightRegistry() {
    Light defaultLight {};
    defaultLight.direction = glm::normalize(glm::vec3 {-0.5F, -1.0F, -0.25F});
    defaultLight.color = glm::vec3 {1.0F, 0.95F, 0.85F};
    defaultLight.intensity = 2.5F;
    m_lights.push_back(defaultLight);
}

LightId LightRegistry::add_light(const Light& light) {
    m_lights.push_back(sanitize_light(light));
    return LightId {static_cast<std::uint32_t>(m_lights.size() - 1U)};
}

void LightRegistry::update_light(LightId id, const Light& light) {
    if (!is_valid(id)) {
        throw std::out_of_range {"LightRegistry::update_light received invalid light id"};
    }
    m_lights[id.value] = sanitize_light(light);
}

void LightRegistry::remove_light(LightId id) {
    if (!is_valid(id)) {
        throw std::out_of_range {"LightRegistry::remove_light received invalid light id"};
    }
    if (id.value == 0U) {
        throw std::invalid_argument {"Cannot remove primary light"};
    }
    m_lights.erase(m_lights.begin() + static_cast<std::ptrdiff_t>(id.value));
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
