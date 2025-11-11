#pragma once

#include <cstdint>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace vulkano::scene {
struct Vertex final {
    glm::vec3 position {};
    glm::vec3 normal {};
    glm::vec3 color {};
    glm::vec2 uv {};
};

struct MeshData final {
    std::vector<Vertex> vertices {};
    std::vector<std::uint32_t> indices {};
};

class MeshFactory final {
public:
    static MeshData create_plane(float size, const glm::vec3& color) noexcept;
    static MeshData create_cube(float size, const glm::vec3& color) noexcept;
    static MeshData create_uv_sphere(float radius, std::uint32_t longitudeSegments,
        std::uint32_t latitudeSegments, const glm::vec3& color);
};
} // namespace vulkano::scene
