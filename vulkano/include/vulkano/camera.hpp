#pragma once

#include <glm/glm.hpp>

namespace vulkano {

class Camera final {
public:
    Camera() = default;
    Camera(glm::vec3 position, glm::vec3 target) noexcept;

    void set_perspective(float fov_radians, float aspect, float near_plane, float far_plane) noexcept;
    void set_view(glm::vec3 position, glm::vec3 target, glm::vec3 up) noexcept;

    [[nodiscard]] const glm::mat4& view() const noexcept;
    [[nodiscard]] const glm::mat4& proj() const noexcept;
    [[nodiscard]] glm::vec3 position() const noexcept;

private:
    glm::mat4 m_view {1.0f};
    glm::mat4 m_proj {1.0f};
    glm::vec3 m_position {0.0f, 0.0f, 3.0f};
};

} // namespace vulkano

