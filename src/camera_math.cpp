#include <vulkano/camera_math.hpp>

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace {
    constexpr float zeroThreshold {1e-6F};
    constexpr glm::vec3 fallbackForward {0.0F, 0.0F, -1.0F};
    constexpr glm::vec3 fallbackRight {1.0F, 0.0F, 0.0F};
    constexpr glm::vec3 fallbackUp {0.0F, 1.0F, 0.0F};
}

namespace vulkano {

[[nodiscard]] auto compute_forward(float yaw, float pitch) noexcept -> glm::vec3 {
    const float cosPitch = std::cos(pitch);
    const float sinPitch = std::sin(pitch);
    const float cosYaw = std::cos(yaw);
    const float sinYaw = std::sin(yaw);

    glm::vec3 forward {cosPitch * cosYaw, sinPitch, cosPitch * sinYaw};
    const float length = glm::length(forward);
    if(length <= zeroThreshold) {
        return fallbackForward;
    }
    return forward / length;
}

[[nodiscard]] auto compute_right(const glm::vec3& forward, const glm::vec3& worldUp) noexcept -> glm::vec3 {
    const glm::vec3 candidate = glm::cross(forward, worldUp);
    const float length = glm::length(candidate);
    if(length <= zeroThreshold) {
        return fallbackRight;
    }
    return candidate / length;
}

[[nodiscard]] auto compute_up(const glm::vec3& forward, const glm::vec3& right) noexcept -> glm::vec3 {
    const glm::vec3 candidate = glm::cross(right, forward);
    const float length = glm::length(candidate);
    if(length <= zeroThreshold) {
        return fallbackUp;
    }
    return candidate / length;
}

[[nodiscard]] auto compute_view_matrix(
    const glm::vec3& position,
    float yaw,
    float pitch,
    const glm::vec3& worldUp) noexcept -> glm::mat4 {
    const glm::vec3 forward = compute_forward(yaw, pitch);
    const glm::vec3 right = compute_right(forward, worldUp);
    const glm::vec3 up = compute_up(forward, right);
    return glm::lookAt(position, position + forward, up);
}

[[nodiscard]] auto clamp_pitch(float pitch, float minPitch, float maxPitch) noexcept -> float {
    return std::clamp(pitch, minPitch, maxPitch);
}

[[nodiscard]] auto adjust_fov(float currentFov, float delta, float minFov, float maxFov) noexcept -> float {
    const float updated = currentFov - delta;
    return std::clamp(updated, minFov, maxFov);
}

} // namespace vulkano
