#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace vulkano::app {
class Camera final {
public:
    enum class Movement {
        Forward,
        Backward,
        Left,
        Right,
        Up,
        Down
    };

    Camera(glm::vec3 position = glm::vec3 {0.0F, 1.5F, 5.0F}, float yawDegrees = -90.0F, float pitchDegrees = 0.0F,
        float aspectRatio = 16.0F / 9.0F, float fovDegrees = 60.0F) noexcept;

    void set_aspect_ratio(float aspect) noexcept;
    void set_fov(float degrees) noexcept;

    void move(Movement movement, float deltaSeconds, float speed) noexcept;
    void rotate(float yawOffsetDegrees, float pitchOffsetDegrees, float sensitivity) noexcept;

    [[nodiscard]] glm::vec3 position() const noexcept;
    void set_position(const glm::vec3& position) noexcept;

    [[nodiscard]] glm::mat4 view_matrix() const noexcept;
    [[nodiscard]] glm::mat4 projection_matrix() const noexcept;

    [[nodiscard]] glm::vec3 forward() const noexcept;
    [[nodiscard]] glm::vec3 right() const noexcept;
    [[nodiscard]] glm::vec3 up() const noexcept;

private:
    void update_orientation() noexcept;
    void update_projection() noexcept;

    glm::vec3 m_position {};
    glm::vec3 m_front {0.0F, 0.0F, -1.0F};
    glm::vec3 m_up {0.0F, 1.0F, 0.0F};
    glm::vec3 m_right {1.0F, 0.0F, 0.0F};

    float m_yawDegrees {-90.0F};
    float m_pitchDegrees {0.0F};
    float m_aspectRatio {16.0F / 9.0F};
    float m_fovDegrees {60.0F};

    glm::mat4 m_projection {1.0F};
};
} // namespace vulkano::app
