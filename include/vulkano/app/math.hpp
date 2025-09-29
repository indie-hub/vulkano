#pragma once

#include <glm/mat4x4.hpp>

namespace vulkano::app {
struct TriangleTransforms final {
    glm::mat4 model {};
    glm::mat4 view {};
    glm::mat4 projection {};
};

TriangleTransforms make_triangle_transforms(float aspectRatio) noexcept;
}
