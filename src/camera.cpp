#include <vulkano/camera.hpp>

#include <cmath>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>

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

void Camera::fov_delta(float dFovRadians) noexcept {
    // Clamp FOV to sensible limits (approx 30..120 degrees)
    constexpr float kMinFov {0.5235987756F};  // 30 deg
    constexpr float kMaxFov {2.09439510239F}; // 120 deg
    params_.fovYRadians += dFovRadians;
    if (params_.fovYRadians < kMinFov) {
        params_.fovYRadians = kMinFov;
    }
    if (params_.fovYRadians > kMaxFov) {
        params_.fovYRadians = kMaxFov;
    }
}

void Camera::look_delta(float dYaw, float dPitch) noexcept {
    // Keep eye position fixed; update yaw/pitch and recompute target accordingly
    const glm::vec3 eye {position()};
    params_.yawRadians += dYaw;
    params_.pitchRadians += dPitch;
    // Clamp pitch to (-89, +89) degrees approx
    constexpr float kMaxPitch {1.55334303F}; // ~89 deg
    if (params_.pitchRadians > kMaxPitch) {
        params_.pitchRadians = kMaxPitch;
    }
    if (params_.pitchRadians < -kMaxPitch) {
        params_.pitchRadians = -kMaxPitch;
    }
    // Recompute target so that position stays the same given new yaw/pitch
    const float cp {std::cos(params_.pitchRadians)};
    const float sp {std::sin(params_.pitchRadians)};
    const float cy {std::cos(params_.yawRadians)};
    const float sy {std::sin(params_.yawRadians)};
    // Direction from target to eye
    const glm::vec3 dir {cp * cy, sp, cp * sy};
    const float dist {params_.distance > 0.0001F ? params_.distance : 0.0001F};
    params_.target = glm::vec3 {eye.x - dir.x * dist, eye.y - dir.y * dist, eye.z - dir.z * dist};
}

void Camera::move_local(const glm::vec3& deltaLocal) noexcept {
    // Move eye in local space, then recompute target to maintain distance and orientation
    const glm::vec3 eye {position()};
    // Forward from eye to target (right-handed)
    const glm::vec3 fwd {forward()};
    const glm::vec3 worldUp {0.0F, 1.0F, 0.0F};
    const glm::vec3 rightV {glm::normalize(glm::cross(fwd, worldUp))};
    const glm::vec3 upV {worldUp};
    const glm::vec3 deltaWorld {rightV * deltaLocal.x + upV * deltaLocal.y + fwd * deltaLocal.z};
    const glm::vec3 newEye {eye + deltaWorld};
    // Recompute target using current yaw/pitch and fixed distance
    const float cp {std::cos(params_.pitchRadians)};
    const float sp {std::sin(params_.pitchRadians)};
    const float cy {std::cos(params_.yawRadians)};
    const float sy {std::sin(params_.yawRadians)};
    const glm::vec3 dir {cp * cy, sp, cp * sy}; // from target to eye
    const float dist {params_.distance > 0.0001F ? params_.distance : 0.0001F};
    params_.target = glm::vec3 {newEye.x - dir.x * dist, newEye.y - dir.y * dist, newEye.z - dir.z * dist};
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

glm::vec3 Camera::forward() const noexcept {
    // Forward from eye to target (right-handed)
    const float cp {std::cos(params_.pitchRadians)};
    const float sp {std::sin(params_.pitchRadians)};
    const float cy {std::cos(params_.yawRadians)};
    const float sy {std::sin(params_.yawRadians)};
    // Direction from target->eye is {cp*cy, sp, cp*sy}; forward is its negative
    const glm::vec3 dirToEye {cp * cy, sp, cp * sy};
    const glm::vec3 fwd {-dirToEye.x, -dirToEye.y, -dirToEye.z};
    return glm::normalize(fwd);
}

glm::vec3 Camera::right() const noexcept {
    const glm::vec3 worldUp {0.0F, 1.0F, 0.0F};
    return glm::normalize(glm::cross(forward(), worldUp));
}

} // namespace vulkano
