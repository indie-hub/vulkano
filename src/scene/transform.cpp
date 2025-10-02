#include <vulkano/scene/transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace vulkano::scene {
namespace {
[[nodiscard]] glm::vec3 degrees_to_radians(const glm::vec3& degrees) noexcept {
    constexpr float factor {glm::pi<float>() / 180.0F};
    return degrees * factor;
}

[[nodiscard]] glm::vec3 radians_to_degrees(const glm::vec3& radians) noexcept {
    constexpr float factor {180.0F / glm::pi<float>()};
    return radians * factor;
}

} // namespace

glm::mat4 Transform::matrix() const noexcept {
    glm::mat4 translation = glm::translate(glm::mat4 {1.0F}, position);
    glm::mat4 rotationMatrix = glm::mat4_cast(glm::normalize(rotation));
    glm::mat4 scaleMatrix = glm::scale(glm::mat4 {1.0F}, scale);
    return translation * rotationMatrix * scaleMatrix;
}

glm::vec3 Transform::euler_degrees() const noexcept {
    const glm::vec3 radians = glm::eulerAngles(glm::normalize(rotation));
    return radians_to_degrees(radians);
}

void Transform::set_euler_degrees(const glm::vec3& degrees) noexcept {
    const glm::vec3 radians = degrees_to_radians(degrees);
    rotation = glm::quat(radians);
    normalize_rotation();
}

void Transform::normalize_rotation() noexcept {
    const float length = glm::length(rotation);
    if (length <= std::numeric_limits<float>::epsilon()) {
        rotation = glm::quat {1.0F, 0.0F, 0.0F, 0.0F};
        return;
    }
    rotation = glm::normalize(rotation);
}

Transform Transform::identity() noexcept {
    Transform transform {};
    transform.position = glm::vec3 {0.0F, 0.0F, 0.0F};
    transform.rotation = glm::quat {1.0F, 0.0F, 0.0F, 0.0F};
    transform.scale = glm::vec3 {1.0F, 1.0F, 1.0F};
    return transform;
}

Transform Transform::from_matrix(const glm::mat4& matrix) noexcept {
    Transform result {};
    glm::vec3 scale {1.0F, 1.0F, 1.0F};
    glm::quat rotation {1.0F, 0.0F, 0.0F, 0.0F};
    glm::vec3 translation {0.0F, 0.0F, 0.0F};
    glm::vec3 skew {0.0F, 0.0F, 0.0F};
    glm::vec4 perspective {0.0F, 0.0F, 0.0F, 1.0F};

    if (!glm::decompose(matrix, scale, rotation, translation, skew, perspective)) {
        return Transform::identity();
    }

    result.position = translation;
    result.scale = scale;
    result.rotation = glm::conjugate(rotation);
    result.normalize_rotation();
    return result;
}
} // namespace vulkano::scene
