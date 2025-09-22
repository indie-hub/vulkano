#include <vulkano/vertex.hpp>

namespace {
    constexpr float triangleHalfWidth {0.5F};
    constexpr float triangleHalfHeight {0.5F};
    constexpr float triangleDepth {0.0F};
}

namespace vulkano {

auto vertex_binding_description() -> VkVertexInputBindingDescription {
    VkVertexInputBindingDescription description {};
    description.binding = 0U;
    description.stride = static_cast<std::uint32_t>(sizeof(Vertex));
    description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return description;
}

auto vertex_attribute_descriptions() -> std::array<VkVertexInputAttributeDescription, 1U> {
    std::array<VkVertexInputAttributeDescription, 1U> descriptions {};
    descriptions[0].binding = 0U;
    descriptions[0].location = 0U;
    descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    descriptions[0].offset = 0U;
    return descriptions;
}

auto default_triangle_vertices() -> VertexArray {
    VertexArray vertices {};
    vertices[0].position = glm::vec3 {0.0F, triangleHalfHeight, triangleDepth};
    vertices[1].position = glm::vec3 {-triangleHalfWidth, -triangleHalfHeight, triangleDepth};
    vertices[2].position = glm::vec3 {triangleHalfWidth, -triangleHalfHeight, triangleDepth};
    for(auto& vertex : vertices) {
        vertex.normal = glm::vec3 {0.0F, 0.0F, 1.0F};
    }
    vertices[0].uv = glm::vec2 {0.5F, 1.0F};
    vertices[1].uv = glm::vec2 {0.0F, 0.0F};
    vertices[2].uv = glm::vec2 {1.0F, 0.0F};
    return vertices;
}

} // namespace vulkano
