#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include <glm/vec3.hpp>

namespace vulkano::scene {
enum class LightType : std::uint32_t {
    Directional = 0U,
    Point = 1U
};

struct LightId final {
    std::uint32_t value {0U};

    [[nodiscard]] constexpr bool operator==(const LightId&) const noexcept = default;
    [[nodiscard]] static constexpr LightId invalid() noexcept {
        return LightId {std::numeric_limits<std::uint32_t>::max()};
    }
};

struct Light final {
    LightType type {LightType::Directional};
    glm::vec3 direction {0.0F, -1.0F, 0.0F};
    glm::vec3 position {0.0F, 2.0F, 0.0F};
    glm::vec3 color {1.0F, 1.0F, 1.0F};
    float intensity {1.0F};
    float range {10.0F};
};

class LightRegistry final {
public:
    LightRegistry();

    [[nodiscard]] LightId add_light(const Light& light);
    void update_light(LightId id, const Light& light);

    [[nodiscard]] const Light& light(LightId id) const;
    [[nodiscard]] Light& light(LightId id);
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const std::vector<Light>& lights() const noexcept;

private:
    [[nodiscard]] bool is_valid(LightId id) const noexcept;

    std::vector<Light> m_lights {};
};
} // namespace vulkano::scene
