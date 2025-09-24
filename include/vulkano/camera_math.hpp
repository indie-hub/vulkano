#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace vulkano {

[[nodiscard]] auto compute_forward(float yaw, float pitch) noexcept -> glm::vec3;
[[nodiscard]] auto compute_right(const glm::vec3& forward, const glm::vec3& worldUp) noexcept -> glm::vec3;
[[nodiscard]] auto compute_up(const glm::vec3& forward, const glm::vec3& right) noexcept -> glm::vec3;
[[nodiscard]] auto compute_view_matrix(const glm::vec3& position, float yaw, float pitch, const glm::vec3& worldUp) noexcept -> glm::mat4;
[[nodiscard]] auto clamp_pitch(float pitch, float minPitch, float maxPitch) noexcept -> float;
[[nodiscard]] auto adjust_fov(float currentFov, float delta, float minFov, float maxFov) noexcept -> float;
[[nodiscard]] auto compute_light_view_projection(
    const glm::vec3& lightPosition,
    const glm::vec3& target,
    const glm::vec3& worldUp,
    float fovY,
    float nearPlane,
    float farPlane,
    const glm::vec3& fallbackDirection,
    float epsilon) noexcept -> glm::mat4;

} // namespace vulkano
