#include <vulkano/app/camera.hpp>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <limits>
#include <glm/trigonometric.hpp>

namespace vulkano::app {
namespace {
constexpr float kPitchLimitDegrees {89.0F};
[[nodiscard]] float clamp_pitch(float pitchDegrees) noexcept {
    return glm::clamp(pitchDegrees, -kPitchLimitDegrees, kPitchLimitDegrees);
}
} // namespace

Camera::Camera(glm::vec3 position, float yawDegrees, float pitchDegrees, float aspectRatio, float fovDegrees) noexcept
    : m_position {position}
    , m_yawDegrees {yawDegrees}
    , m_pitchDegrees {clamp_pitch(pitchDegrees)}
    , m_aspectRatio {aspectRatio}
    , m_fovDegrees {fovDegrees} {
    update_orientation();
    update_projection();
}

void Camera::set_aspect_ratio(float aspect) noexcept {
    if (aspect <= 0.0F) {
        return;
    }
    if (glm::abs(aspect - m_aspectRatio) < std::numeric_limits<float>::epsilon()) {
        return;
    }
    m_aspectRatio = aspect;
    update_projection();
}

void Camera::set_fov(float degrees) noexcept {
    m_fovDegrees = glm::clamp(degrees, 1.0F, 120.0F);
    update_projection();
}

void Camera::move(Movement movement, float deltaSeconds, float speed) noexcept {
    const float velocity = speed * deltaSeconds;
    switch (movement) {
    case Movement::Forward:
        m_position += m_front * velocity;
        break;
    case Movement::Backward:
        m_position -= m_front * velocity;
        break;
    case Movement::Left:
        m_position -= m_right * velocity;
        break;
    case Movement::Right:
        m_position += m_right * velocity;
        break;
    case Movement::Up:
        m_position += m_up * velocity;
        break;
    case Movement::Down:
        m_position -= m_up * velocity;
        break;
    }
}

void Camera::rotate(float yawOffsetDegrees, float pitchOffsetDegrees, float sensitivity) noexcept {
    m_yawDegrees += yawOffsetDegrees * sensitivity;
    m_pitchDegrees = clamp_pitch(m_pitchDegrees + pitchOffsetDegrees * sensitivity);
    update_orientation();
}

glm::vec3 Camera::position() const noexcept {
    return m_position;
}

void Camera::set_position(const glm::vec3& position) noexcept {
    m_position = position;
}

glm::mat4 Camera::view_matrix() const noexcept {
    return glm::lookAt(m_position, m_position + m_front, glm::vec3 {0.0F, 1.0F, 0.0F});
}

glm::mat4 Camera::projection_matrix() const noexcept {
    return m_projection;
}

glm::vec3 Camera::forward() const noexcept {
    return m_front;
}

glm::vec3 Camera::right() const noexcept {
    return m_right;
}

glm::vec3 Camera::up() const noexcept {
    return m_up;
}

void Camera::update_orientation() noexcept {
    const float yawRadians = glm::radians(m_yawDegrees);
    const float pitchRadians = glm::radians(m_pitchDegrees);

    glm::vec3 front {};
    front.x = glm::cos(pitchRadians) * glm::cos(yawRadians);
    front.y = glm::sin(pitchRadians);
    front.z = glm::cos(pitchRadians) * glm::sin(yawRadians);
    m_front = glm::normalize(front);

    m_right = glm::normalize(glm::cross(m_front, glm::vec3 {0.0F, 1.0F, 0.0F}));
    m_up = glm::normalize(glm::cross(m_right, m_front));
}

void Camera::update_projection() noexcept {
    m_projection = glm::perspective(glm::radians(m_fovDegrees), m_aspectRatio, 0.1F, 100.0F);
    m_projection[1][1] *= -1.0F; // compensate for Vulkan clip space
}
} // namespace vulkano::app
