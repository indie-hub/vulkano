#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

namespace vulkano::scene {
class Transform final {
public:
    glm::vec3 position {0.0F, 0.0F, 0.0F};
    glm::quat rotation {1.0F, 0.0F, 0.0F, 0.0F};
    glm::vec3 scale {1.0F, 1.0F, 1.0F};

    [[nodiscard]] glm::mat4 matrix() const noexcept;
    [[nodiscard]] glm::vec3 euler_degrees() const noexcept;
    void set_euler_degrees(const glm::vec3& degrees) noexcept;
    void normalize_rotation() noexcept;

    [[nodiscard]] static Transform identity() noexcept;
    [[nodiscard]] static Transform from_matrix(const glm::mat4& matrix) noexcept;
};
} // namespace vulkano::scene

