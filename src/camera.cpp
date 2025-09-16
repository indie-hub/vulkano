#include <vulkano/camera.hpp>

#include <cmath>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace vulkano {

Camera::Camera() noexcept : params_ {} {
}

Camera::Camera(const Params& p) noexcept : params_ {p} {
}

void Camera::set_params(const Params& p) noexcept {
    params_ = p;
}

const Camera::Params& Camera::params() const noexcept {
    return params_;
}

void Camera::set_aspect(float aspect) noexcept {
    if (aspect > 0.0F) {
        params_.aspect = aspect;
    }
}

void Camera::orbit_delta(float dYaw, float dPitch, float dDistance) noexcept {
    params_.yawRadians += dYaw;
    params_.pitchRadians += dPitch;
    // Clamp pitch to avoid gimbal flip; limit to (-89, +89) deg approx
    const float maxPitch {1.55334303F}; // ~89 deg
    if (params_.pitchRadians > maxPitch) {
        params_.pitchRadians = maxPitch;
    }
    if (params_.pitchRadians < -maxPitch) {
        params_.pitchRadians = -maxPitch;
    }
    params_.distance += dDistance;
    if (params_.distance < 0.1F) {
        params_.distance = 0.1F;
    }
}

void Camera::pan_delta(const glm::vec3& delta) noexcept {
    params_.target = glm::vec3 {params_.target.x + delta.x, params_.target.y + delta.y, params_.target.z + delta.z};
}

glm::vec3 Camera::position() const noexcept {
    const float cp {std::cos(params_.pitchRadians)};
    const float sp {std::sin(params_.pitchRadians)};
    const float cy {std::cos(params_.yawRadians)};
    const float sy {std::sin(params_.yawRadians)};
    const float x {params_.target.x + params_.distance * cp * cy};
    const float y {params_.target.y + params_.distance * sp};
    const float z {params_.target.z + params_.distance * cp * sy};
    return glm::vec3 {x, y, z};
}

glm::mat4 Camera::view() const noexcept {
    const glm::vec3 eye {position()};
    const glm::vec3 up {0.0F, 1.0F, 0.0F};
    return glm::lookAtRH(eye, params_.target, up);
}

glm::mat4 Camera::projection() const noexcept {
    // Vulkan NDC depth: 0..1
    return glm::perspectiveRH_ZO(params_.fovYRadians, params_.aspect, params_.nearPlane, params_.farPlane);
}

glm::mat4 Camera::view_projection() const noexcept {
    return projection() * view();
}

} // namespace vulkano

