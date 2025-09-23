#pragma once

#include <array>
#include <cstddef>
#include <vulkan/vulkan.h>

#include <vulkano/mesh.hpp>

namespace vulkano {

using Vertex = MeshVertex;
using VertexArray = std::array<Vertex, 3U>;

[[nodiscard]] auto vertex_binding_description() -> VkVertexInputBindingDescription;
[[nodiscard]] auto vertex_attribute_descriptions() -> std::array<VkVertexInputAttributeDescription, 4U>;
[[nodiscard]] auto shadow_vertex_attribute_descriptions()
    -> std::array<VkVertexInputAttributeDescription, 1U>;
[[nodiscard]] auto default_triangle_vertices() -> VertexArray;

} // namespace vulkano
