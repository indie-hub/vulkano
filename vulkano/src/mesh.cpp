#include <vulkano/mesh.hpp>

namespace vulkano {

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<std::uint32_t> indices) noexcept
    : m_vertices {std::move(vertices)}
    , m_indices {std::move(indices)}
{
}

const std::vector<Vertex>& Mesh::vertices() const noexcept
{
    return m_vertices;
}

const std::vector<std::uint32_t>& Mesh::indices() const noexcept
{
    return m_indices;
}

std::size_t Mesh::vertex_count() const noexcept
{
    return m_vertices.size();
}

std::size_t Mesh::index_count() const noexcept
{
    return m_indices.size();
}

void compute_tangents(std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices) noexcept
{
    for (auto& v : vertices) {
        v.tangent = glm::vec3 {0.0f, 0.0f, 0.0f};
        v.bitangent = glm::vec3 {0.0f, 0.0f, 0.0f};
    }

    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        const auto i0 = indices[i + 0];
        const auto i1 = indices[i + 1];
        const auto i2 = indices[i + 2];

        const auto& v0 = vertices[i0];
        const auto& v1 = vertices[i1];
        const auto& v2 = vertices[i2];

        const glm::vec3 e1 = v1.position - v0.position;
        const glm::vec3 e2 = v2.position - v0.position;
        const glm::vec2 dUV1 = v1.uv - v0.uv;
        const glm::vec2 dUV2 = v2.uv - v0.uv;

        const float denom = dUV1.x * dUV2.y - dUV2.x * dUV1.y;
        if (denom == 0.0f) {
            continue;
        }
        const float r = 1.0f / denom;
        const glm::vec3 T = (e1 * dUV2.y - e2 * dUV1.y) * r;
        const glm::vec3 B = (e2 * dUV1.x - e1 * dUV2.x) * r;

        vertices[i0].tangent += T;
        vertices[i1].tangent += T;
        vertices[i2].tangent += T;
        vertices[i0].bitangent += B;
        vertices[i1].bitangent += B;
        vertices[i2].bitangent += B;
    }

    for (auto& v : vertices) {
        const glm::vec3 n = glm::normalize(v.normal);
        glm::vec3 t = v.tangent;
        t = glm::normalize(t - n * glm::dot(n, t));
        glm::vec3 b = glm::cross(n, t);
        v.normal = n;
        v.tangent = t;
        v.bitangent = b;
    }
}

} // namespace vulkano

