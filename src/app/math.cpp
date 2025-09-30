#include <vulkano/app/math.hpp>

#include <cmath>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/glm.hpp>

namespace vulkano::app {
TriangleTransforms make_triangle_transforms(float aspectRatio) noexcept {
    constexpr glm::vec3 eye {0.0F, 0.0F, 2.0F};
    constexpr glm::vec3 center {0.0F, 0.0F, 0.0F};
    constexpr glm::vec3 up {0.0F, 1.0F, 0.0F};

    TriangleTransforms transforms {};
    transforms.model = glm::mat4 {1.0F};
    transforms.view = glm::lookAtRH(eye, center, up);
    transforms.projection = glm::perspectiveRH_ZO(glm::radians(60.0F), aspectRatio, 0.1F, 10.0F);
    transforms.projection[1][1] *= -1.0F;
    return transforms;
}

glm::vec3 transform_normal_to_view(const glm::mat4& model, const glm::mat4& view, const glm::vec3& normal) noexcept {
    const glm::mat3 modelNormal = glm::transpose(glm::inverse(glm::mat3 {model}));
    const glm::vec3 worldNormal = glm::normalize(modelNormal * normal);
    const glm::vec3 viewNormal = glm::normalize(glm::mat3 {view} * worldNormal);
    return viewNormal;
}

glm::vec3 reconstruct_view_position_from_depth(
    const glm::mat4& inverseProjection, const glm::vec2& uv, float linearDepth) noexcept {
    if (linearDepth <= 0.0F) {
        return glm::vec3 {0.0F};
    }

    const glm::vec2 ndc = uv * 2.0F - glm::vec2 {1.0F};
    const glm::vec4 clip {ndc, 1.0F, 1.0F};
    const glm::vec4 view = inverseProjection * clip;
    glm::vec3 viewDir = glm::vec3 {view} / view.w;
    float denom = viewDir.z;
    if (std::abs(denom) < 1e-4F) {
        denom = denom < 0.0F ? -1e-4F : 1e-4F;
    }
    const float scale = -linearDepth / denom;
    return viewDir * scale;
}
} // namespace vulkano::app
