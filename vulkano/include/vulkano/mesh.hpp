#pragma once

#include <array>
#include <vector>
#include <glm/glm.hpp>

namespace vulkano {

struct Vertex final {
    glm::vec3 position {};
    glm::vec3 normal {};
    glm::vec3 tangent {};
    glm::vec3 bitangent {};
    glm::vec2 uv {};
};

class Mesh final {
public:
    Mesh() = default;
    explicit Mesh(std::vector<Vertex> vertices, std::vector<std::uint32_t> indices) noexcept;

    const std::vector<Vertex>& vertices() const noexcept;
    const std::vector<std::uint32_t>& indices() const noexcept;
    std::size_t vertex_count() const noexcept;
    std::size_t index_count() const noexcept;

private:
    std::vector<Vertex> m_vertices {};
    std::vector<std::uint32_t> m_indices {};
};

void compute_tangents(std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices) noexcept;

} // namespace vulkano

