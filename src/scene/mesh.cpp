#include <vulkano/scene/mesh.hpp>

#include <stdexcept>
#include <array>
#include <cmath>

#include <glm/gtc/constants.hpp>
#include <glm/geometric.hpp>

namespace vulkano::scene {
namespace {
[[nodiscard]] Vertex make_vertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec3& color) noexcept {
    Vertex vertex {};
    vertex.position = position;
    vertex.normal = normal;
    vertex.color = color;
    return vertex;
}
}

MeshData MeshFactory::create_plane(float size, const glm::vec3& color) noexcept {
    const float halfSize {size * 0.5F};
    MeshData data {};
    data.vertices = {
        make_vertex(glm::vec3 {-halfSize, 0.0F, -halfSize}, glm::vec3 {0.0F, 1.0F, 0.0F}, color),
        make_vertex(glm::vec3 {halfSize, 0.0F, -halfSize}, glm::vec3 {0.0F, 1.0F, 0.0F}, color),
        make_vertex(glm::vec3 {halfSize, 0.0F, halfSize}, glm::vec3 {0.0F, 1.0F, 0.0F}, color),
        make_vertex(glm::vec3 {-halfSize, 0.0F, halfSize}, glm::vec3 {0.0F, 1.0F, 0.0F}, color)
    };
    data.indices = {0U, 2U, 1U, 0U, 3U, 2U};
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

    MeshData data {};
    data.vertices.reserve(24U);
    data.indices.reserve(36U);

    for (std::size_t face {0U}; face < positions.size(); ++face) {
        const std::uint32_t baseIndex {static_cast<std::uint32_t>(data.vertices.size())};
        for (const glm::vec3& position : positions[face]) {
            data.vertices.push_back(make_vertex(position, normals[face], color));
        }
        data.indices.insert(data.indices.end(), {baseIndex, baseIndex + 1U, baseIndex + 2U,
            baseIndex, baseIndex + 2U, baseIndex + 3U});
    }

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
            data.vertices.push_back(make_vertex(position, glm::normalize(normal), color));
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

    return data;
}
} // namespace vulkano::scene
