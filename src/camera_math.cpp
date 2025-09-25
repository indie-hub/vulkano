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

[[nodiscard]] auto compute_light_view_projection(
    const glm::vec3& lightPosition,
    const glm::vec3& target,
    const glm::vec3& worldUp,
    float fovY,
    float nearPlane,
    float farPlane,
    const glm::vec3& fallbackDirection,
    float epsilon) noexcept -> glm::mat4 {
    glm::vec3 adjustedTarget = target;
    if(glm::length(target - lightPosition) <= epsilon) {
        glm::vec3 safeFallback = fallbackDirection;
        if(glm::length(safeFallback) <= epsilon) {
            safeFallback = fallbackForward;
        }
        const glm::vec3 normalizedFallback = glm::normalize(safeFallback);
        const glm::vec3 normalizedUp = glm::normalize(worldUp);
        if(glm::abs(glm::dot(normalizedFallback, normalizedUp)) >= 1.0F - epsilon) {
            safeFallback = fallbackForward;
        }
        adjustedTarget = lightPosition + safeFallback;
    }
    const glm::mat4 view = glm::lookAt(lightPosition, adjustedTarget, worldUp);
    const glm::mat4 projection = glm::perspective(fovY, 1.0F, nearPlane, farPlane);
    return projection * view;
}

[[nodiscard]] auto compute_cascade_splits(
    float nearPlane,
    float farPlane,
    std::uint32_t cascadeCount,
    float lambda) noexcept -> std::array<float, maxShadowCascades> {
    std::array<float, maxShadowCascades> splits {};
    if(cascadeCount == 0U) {
        return splits;
    }

    const float clampedLambda = std::clamp(lambda, 0.0F, 1.0F);
    const float minZ = std::max(nearPlane, 0.0001F);
    const float maxZ = std::max(farPlane, minZ + 0.0001F);
    const float range = maxZ - minZ;

    for(std::uint32_t index {0U}; index < cascadeCount; ++index) {
        const float fraction = static_cast<float>(index + 1U) / static_cast<float>(cascadeCount);
        const float logSplit = minZ * std::pow(maxZ / minZ, fraction);
        const float linearSplit = minZ + range * fraction;
        const float split = std::lerp(linearSplit, logSplit, clampedLambda);
        splits.at(index) = (split - minZ) / range;
    }

    for(std::uint32_t index {cascadeCount}; index < maxShadowCascades; ++index) {
        splits.at(index) = 1.0F;
    }

    return splits;
}

[[nodiscard]] auto select_cascade(
    float depthNormalized,
    const std::array<float, maxShadowCascades>& splits,
    std::uint32_t cascadeCount) noexcept -> std::uint32_t {
    const std::uint32_t clampedCount = std::max(1U, std::min<std::uint32_t>(cascadeCount, static_cast<std::uint32_t>(maxShadowCascades)));
    const float depth = std::clamp(depthNormalized, 0.0F, 1.0F);
    for(std::uint32_t index {0U}; index < clampedCount; ++index) {
        if(depth <= splits.at(index)) {
            return index;
        }
    }
    return clampedCount - 1U;
}

} // namespace vulkano
