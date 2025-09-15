#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <vulkan_app/camera.hpp>

namespace vulkan_app {

glm::mat4 Camera::view() const noexcept {
    const glm::vec3 forward{
        std::cos(pitch_) * std::cos(yaw_),
        std::sin(pitch_),
        std::cos(pitch_) * std::sin(yaw_)
    };
    const glm::vec3 target{position_ + glm::normalize(forward)};
    return glm::lookAt(position_, target, glm::vec3{0.0F, 1.0F, 0.0F});
}

glm::mat4 Camera::proj(const float aspect) const noexcept {
    glm::mat4 p{glm::perspective(fov_, aspect, near_, far_)};
    // Vulkan clip space has inverted Y.
    p[1][1] *= -1.0F;
    return p;
}

} // namespace vulkan_app

