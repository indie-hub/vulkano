#include <vulkano/camera.hpp>

#include <glm/gtc/matrix_transform.hpp>

namespace vulkano {

Camera::Camera(glm::vec3 position, glm::vec3 target) noexcept
    : m_position {position}
{
    set_view(position, target, glm::vec3 {0.0f, 1.0f, 0.0f});
}

void Camera::set_perspective(const float fov_radians, const float aspect, const float near_plane, const float far_plane) noexcept
{
    m_proj = glm::perspective(fov_radians, aspect, near_plane, far_plane);
    m_proj[1][1] *= -1.0f; // GLM is OpenGL-style; flip for Vulkan
}

void Camera::set_view(const glm::vec3 position, const glm::vec3 target, const glm::vec3 up) noexcept
{
    m_position = position;
    m_view = glm::lookAt(position, target, up);
}

const glm::mat4& Camera::view() const noexcept
{
    return m_view;
}

const glm::mat4& Camera::proj() const noexcept
{
    return m_proj;
}

glm::vec3 Camera::position() const noexcept
{
    return m_position;
}

} // namespace vulkano

