#include <vulkano/scene/mesh.hpp>

#include <stdexcept>
#include <array>
#include <cmath>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/geometric.hpp>

namespace vulkano::scene {
namespace {
constexpr float TANGENT_EPSILON {1e-6F};

[[nodiscard]] glm::vec3 fallback_tangent(const glm::vec3& normal) noexcept {
    glm::vec3 tangent = glm::cross(normal, glm::vec3 {0.0F, 0.0F, 1.0F});
    if (glm::dot(tangent, tangent) <= TANGENT_EPSILON) {
        tangent = glm::cross(normal, glm::vec3 {0.0F, 1.0F, 0.0F});
    }
    if (glm::dot(tangent, tangent) <= TANGENT_EPSILON) {
        tangent = glm::cross(normal, glm::vec3 {1.0F, 0.0F, 0.0F});
    }
    const float lengthSquared = glm::dot(tangent, tangent);
    if (lengthSquared <= TANGENT_EPSILON) {
        return glm::vec3 {1.0F, 0.0F, 0.0F};
    }
    return glm::normalize(tangent);
}

void compute_tangent_frames(std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices) noexcept {
    if (vertices.empty()) {
        return;
    }

    std::vector<glm::vec3> tangents(vertices.size(), glm::vec3 {0.0F, 0.0F, 0.0F});
    std::vector<glm::vec3> bitangents(vertices.size(), glm::vec3 {0.0F, 0.0F, 0.0F});

    const auto accumulateTriangle = [&](std::uint32_t i0, std::uint32_t i1, std::uint32_t i2) noexcept {
        const Vertex& v0 = vertices[i0];
        const Vertex& v1 = vertices[i1];
        const Vertex& v2 = vertices[i2];

        const glm::vec3 edge1 = v1.position - v0.position;
        const glm::vec3 edge2 = v2.position - v0.position;
        const glm::vec2 deltaUV1 = v1.uv - v0.uv;
        const glm::vec2 deltaUV2 = v2.uv - v0.uv;

        const float determinant = deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x;
        if (std::abs(determinant) <= TANGENT_EPSILON) {
            return;
        }

        const float inverseDeterminant = 1.0F / determinant;
        const glm::vec3 tangent = (edge1 * deltaUV2.y - edge2 * deltaUV1.y) * inverseDeterminant;
        const glm::vec3 bitangent = (edge2 * deltaUV1.x - edge1 * deltaUV2.x) * inverseDeterminant;

        tangents[i0] += tangent;
        tangents[i1] += tangent;
        tangents[i2] += tangent;
        bitangents[i0] += bitangent;
        bitangents[i1] += bitangent;
        bitangents[i2] += bitangent;
    };

    if (!indices.empty()) {
        for (std::size_t triangle {0U}; triangle + 2U < indices.size(); triangle += 3U) {
            const std::uint32_t i0 = indices[triangle];
            const std::uint32_t i1 = indices[triangle + 1U];
            const std::uint32_t i2 = indices[triangle + 2U];
            if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
                continue;
            }
            accumulateTriangle(i0, i1, i2);
        }
    } else {
        for (std::size_t triangle {0U}; triangle + 2U < vertices.size(); triangle += 3U) {
            accumulateTriangle(static_cast<std::uint32_t>(triangle), static_cast<std::uint32_t>(triangle + 1U),
                static_cast<std::uint32_t>(triangle + 2U));
        }
    }

    for (std::size_t index {0U}; index < vertices.size(); ++index) {
        Vertex& vertex = vertices[index];
        const float normalLengthSquared = glm::dot(vertex.normal, vertex.normal);
        glm::vec3 normal = vertex.normal;
        if (normalLengthSquared <= TANGENT_EPSILON) {
            normal = glm::vec3 {0.0F, 1.0F, 0.0F};
        } else {
            normal = glm::normalize(vertex.normal);
        }

        glm::vec3 tangent = tangents[index];
        const float tangentLengthSquared = glm::dot(tangent, tangent);
        if (tangentLengthSquared <= TANGENT_EPSILON) {
            tangent = fallback_tangent(normal);
        } else {
            tangent = glm::normalize(tangent - normal * glm::dot(normal, tangent));
            const float correctedLengthSquared = glm::dot(tangent, tangent);
            if (correctedLengthSquared <= TANGENT_EPSILON) {
                tangent = fallback_tangent(normal);
            }
        }

        glm::vec3 bitangent = bitangents[index];
        float bitangentSign {1.0F};
        const float bitangentLengthSquared = glm::dot(bitangent, bitangent);
        if (bitangentLengthSquared > TANGENT_EPSILON) {
            bitangent = glm::normalize(bitangent);
            const float handedness = glm::dot(glm::cross(normal, tangent), bitangent);
            bitangentSign = handedness < 0.0F ? -1.0F : 1.0F;
        }

        vertex.tangent = tangent;
        vertex.bitangentSign = bitangentSign;
    }
}

[[nodiscard]] Vertex make_vertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec3& color,
    const glm::vec2& uv) noexcept {
    Vertex vertex {};
    vertex.position = position;
    vertex.normal = normal;
    vertex.color = color;
    vertex.uv = uv;
    vertex.tangent = glm::vec3 {0.0F, 0.0F, 0.0F};
    vertex.bitangentSign = 1.0F;
    return vertex;
}
}

MeshData MeshFactory::create_plane(float size, const glm::vec3& color) noexcept {
    const float halfSize {size * 0.5F};
    MeshData data {};
    data.vertices = {
        make_vertex(glm::vec3 {-halfSize, 0.0F, -halfSize}, glm::vec3 {0.0F, 1.0F, 0.0F}, color, glm::vec2 {0.0F, 0.0F}),
        make_vertex(glm::vec3 {halfSize, 0.0F, -halfSize}, glm::vec3 {0.0F, 1.0F, 0.0F}, color, glm::vec2 {1.0F, 0.0F}),
        make_vertex(glm::vec3 {halfSize, 0.0F, halfSize}, glm::vec3 {0.0F, 1.0F, 0.0F}, color, glm::vec2 {1.0F, 1.0F}),
        make_vertex(glm::vec3 {-halfSize, 0.0F, halfSize}, glm::vec3 {0.0F, 1.0F, 0.0F}, color, glm::vec2 {0.0F, 1.0F})
    };
    data.indices = {0U, 2U, 1U, 0U, 3U, 2U};
    compute_tangent_frames(data.vertices, data.indices);
    return data;
}

MeshData MeshFactory::create_cube(float size, const glm::vec3& color) noexcept {
    const float half {size * 0.5F};
    const std::array<glm::vec3, 6> normals {
        glm::vec3 {0.0F, 0.0F, 1.0F},  // front
        glm::vec3 {0.0F, 0.0F, -1.0F}, // back
        glm::vec3 {0.0F, 1.0F, 0.0F},  // top
        glm::vec3 {0.0F, -1.0F, 0.0F}, // bottom
        glm::vec3 {1.0F, 0.0F, 0.0F},  // right
        glm::vec3 {-1.0F, 0.0F, 0.0F}  // left
    };

    const std::array<std::array<glm::vec3, 4>, 6> positions {
        std::array<glm::vec3, 4> { // front
            glm::vec3 {-half, -half, half},
            glm::vec3 {half, -half, half},
            glm::vec3 {half, half, half},
            glm::vec3 {-half, half, half}},
        std::array<glm::vec3, 4> { // back
            glm::vec3 {half, -half, -half},
            glm::vec3 {-half, -half, -half},
            glm::vec3 {-half, half, -half},
            glm::vec3 {half, half, -half}},
        std::array<glm::vec3, 4> { // top
            glm::vec3 {-half, half, half},
            glm::vec3 {half, half, half},
            glm::vec3 {half, half, -half},
            glm::vec3 {-half, half, -half}},
        std::array<glm::vec3, 4> { // bottom
            glm::vec3 {-half, -half, -half},
            glm::vec3 {half, -half, -half},
            glm::vec3 {half, -half, half},
            glm::vec3 {-half, -half, half}},
        std::array<glm::vec3, 4> { // right
            glm::vec3 {half, -half, half},
            glm::vec3 {half, -half, -half},
            glm::vec3 {half, half, -half},
            glm::vec3 {half, half, half}},
        std::array<glm::vec3, 4> { // left
            glm::vec3 {-half, -half, -half},
            glm::vec3 {-half, -half, half},
            glm::vec3 {-half, half, half},
            glm::vec3 {-half, half, -half}}
    };

    const std::array<std::array<glm::vec2, 4>, 6> uvs {
        std::array<glm::vec2, 4> {{glm::vec2 {0.0F, 0.0F}, glm::vec2 {1.0F, 0.0F}, glm::vec2 {1.0F, 1.0F}, glm::vec2 {0.0F, 1.0F}}},
        std::array<glm::vec2, 4> {{glm::vec2 {1.0F, 0.0F}, glm::vec2 {0.0F, 0.0F}, glm::vec2 {0.0F, 1.0F}, glm::vec2 {1.0F, 1.0F}}},
        std::array<glm::vec2, 4> {{glm::vec2 {0.0F, 1.0F}, glm::vec2 {1.0F, 1.0F}, glm::vec2 {1.0F, 0.0F}, glm::vec2 {0.0F, 0.0F}}},
        std::array<glm::vec2, 4> {{glm::vec2 {0.0F, 0.0F}, glm::vec2 {1.0F, 0.0F}, glm::vec2 {1.0F, 1.0F}, glm::vec2 {0.0F, 1.0F}}},
        std::array<glm::vec2, 4> {{glm::vec2 {0.0F, 0.0F}, glm::vec2 {1.0F, 0.0F}, glm::vec2 {1.0F, 1.0F}, glm::vec2 {0.0F, 1.0F}}},
        std::array<glm::vec2, 4> {{glm::vec2 {1.0F, 0.0F}, glm::vec2 {0.0F, 0.0F}, glm::vec2 {0.0F, 1.0F}, glm::vec2 {1.0F, 1.0F}}}
    };

    MeshData data {};
    data.vertices.reserve(24U);
    data.indices.reserve(36U);

    for (std::size_t face {0U}; face < positions.size(); ++face) {
        const std::uint32_t baseIndex {static_cast<std::uint32_t>(data.vertices.size())};
        for (std::size_t corner {0U}; corner < 4U; ++corner) {
            data.vertices.push_back(make_vertex(positions[face][corner], normals[face], color, uvs[face][corner]));
        }
        data.indices.insert(data.indices.end(), {baseIndex, baseIndex + 1U, baseIndex + 2U,
            baseIndex, baseIndex + 2U, baseIndex + 3U});
    }

    compute_tangent_frames(data.vertices, data.indices);
    return data;
}

MeshData MeshFactory::create_uv_sphere(float radius, std::uint32_t longitudeSegments,
    std::uint32_t latitudeSegments, const glm::vec3& color) {
    if (longitudeSegments < 3U || latitudeSegments < 2U) {
        throw std::invalid_argument {"Sphere requires at least 3 longitude and 2 latitude segments"};
    }

    MeshData data {};
    const std::uint32_t verticalCount {latitudeSegments + 1U};
    const std::uint32_t horizontalCount {longitudeSegments + 1U};

    data.vertices.reserve(static_cast<std::size_t>(verticalCount) * static_cast<std::size_t>(horizontalCount));
    for (std::uint32_t y {0U}; y < verticalCount; ++y) {
        const float v {static_cast<float>(y) / static_cast<float>(latitudeSegments)};
        const float theta {v * glm::pi<float>()};
        const float sinTheta {std::sin(theta)};
        const float cosTheta {std::cos(theta)};

        for (std::uint32_t x {0U}; x < horizontalCount; ++x) {
            const float u {static_cast<float>(x) / static_cast<float>(longitudeSegments)};
            const float phi {u * glm::two_pi<float>()};
            const float sinPhi {std::sin(phi)};
            const float cosPhi {std::cos(phi)};

            glm::vec3 normal {sinTheta * cosPhi, cosTheta, sinTheta * sinPhi};
            glm::vec3 position {radius * normal};
            glm::vec2 uv {u, 1.0F - v};
            data.vertices.push_back(make_vertex(position, glm::normalize(normal), color, uv));
        }
    }

    for (std::uint32_t y {0U}; y < latitudeSegments; ++y) {
        for (std::uint32_t x {0U}; x < longitudeSegments; ++x) {
            const std::uint32_t index0 {y * horizontalCount + x};
            const std::uint32_t index1 {index0 + horizontalCount};

            data.indices.insert(data.indices.end(), {
                index0,
                index1,
                index0 + 1U,
                index0 + 1U,
                index1,
                index1 + 1U
            });
        }
    }

    compute_tangent_frames(data.vertices, data.indices);
    return data;
}

void generate_tangent_frames(MeshData& mesh) noexcept {
    compute_tangent_frames(mesh.vertices, mesh.indices);
}
} // namespace vulkano::scene
