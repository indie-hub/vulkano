#include <vulkano/app/math.hpp>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
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
} // namespace vulkano::app
