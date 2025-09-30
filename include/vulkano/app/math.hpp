#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace vulkano::app {
struct TriangleTransforms final {
    glm::mat4 model {};
    glm::mat4 view {};
    glm::mat4 projection {};
};

TriangleTransforms make_triangle_transforms(float aspectRatio) noexcept;
glm::vec3 transform_normal_to_view(const glm::mat4& model, const glm::mat4& view, const glm::vec3& normal) noexcept;
glm::vec3 reconstruct_view_position_from_depth(
    const glm::mat4& inverseProjection, const glm::vec2& uv, float linearDepth) noexcept;
}
