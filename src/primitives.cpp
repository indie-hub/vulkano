#include <vulkano/primitives.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>

namespace {
    constexpr std::string_view planeIdentifier {"Plane"};
    constexpr std::string_view cubeIdentifier {"Cube"};
    constexpr std::string_view icosphereIdentifier {"Icosphere"};
    constexpr float planeMinimumExtent {0.0001F};
    constexpr float defaultCubeHalfExtent {0.5F};
    constexpr std::uint32_t minIcosphereSubdivision {0U};
    constexpr std::uint32_t maxIcosphereSubdivision {5U};

    constexpr std::array<glm::vec2, 4U> faceCornerOffsets {
        glm::vec2 {1.0F, -1.0F},
        glm::vec2 {-1.0F, -1.0F},
        glm::vec2 {-1.0F, 1.0F},
        glm::vec2 {1.0F, 1.0F}
    };

    constexpr std::array<glm::vec2, 4U> faceCornerUvs {
        glm::vec2 {0.0F, 1.0F},
        glm::vec2 {0.0F, 0.0F},
        glm::vec2 {1.0F, 0.0F},
        glm::vec2 {1.0F, 1.0F}
    };

    struct FaceDefinition final {
        glm::vec3 normal {};
        glm::vec3 up {};
        glm::vec3 right {};
    };

    constexpr std::array<FaceDefinition, 6U> cubeFaces {
        FaceDefinition {glm::vec3 {1.0F, 0.0F, 0.0F}, glm::vec3 {0.0F, 1.0F, 0.0F}, glm::vec3 {0.0F, 0.0F, -1.0F}},
        FaceDefinition {glm::vec3 {-1.0F, 0.0F, 0.0F}, glm::vec3 {0.0F, 1.0F, 0.0F}, glm::vec3 {0.0F, 0.0F, 1.0F}},
        FaceDefinition {glm::vec3 {0.0F, 1.0F, 0.0F}, glm::vec3 {0.0F, 0.0F, -1.0F}, glm::vec3 {1.0F, 0.0F, 0.0F}},
        FaceDefinition {glm::vec3 {0.0F, -1.0F, 0.0F}, glm::vec3 {0.0F, 0.0F, 1.0F}, glm::vec3 {1.0F, 0.0F, 0.0F}},
        FaceDefinition {glm::vec3 {0.0F, 0.0F, 1.0F}, glm::vec3 {0.0F, 1.0F, 0.0F}, glm::vec3 {1.0F, 0.0F, 0.0F}},
        FaceDefinition {glm::vec3 {0.0F, 0.0F, -1.0F}, glm::vec3 {0.0F, 1.0F, 0.0F}, glm::vec3 {-1.0F, 0.0F, 0.0F}}
    };

    constexpr std::array<std::uint32_t, 6U> faceIndicesPattern {0U, 1U, 2U, 0U, 2U, 3U};

    auto validate_plane_parameters(const vulkano::PlaneParameters& parameters) -> void {
        if(parameters.width < planeMinimumExtent || parameters.depth < planeMinimumExtent) {
            throw std::invalid_argument {"Plane dimensions must be positive"};
        }
    }

    auto validate_subdivisions(std::uint32_t subdivisions) -> void {
        if(subdivisions < minIcosphereSubdivision || subdivisions > maxIcosphereSubdivision) {
            throw std::invalid_argument {"Icosphere subdivisions out of supported range"};
        }
    }

    auto edge_key(std::uint32_t first, std::uint32_t second) -> std::uint64_t {
        const std::uint32_t minValue = std::min(first, second);
        const std::uint32_t maxValue = std::max(first, second);
        return (static_cast<std::uint64_t>(minValue) << 32U) | static_cast<std::uint64_t>(maxValue);
    }

    void compute_tangents(vulkano::MeshData& mesh) {
        using namespace vulkano;
        if(mesh.vertices.empty() || mesh.indices.size() < 3U) {
            return;
        }

        std::vector<glm::vec3> tangents(mesh.vertices.size(), glm::vec3 {0.0F});
        std::vector<glm::vec3> bitangents(mesh.vertices.size(), glm::vec3 {0.0F});

        constexpr float epsilon {1e-6F};

        for(std::size_t index {0U}; index + 2U < mesh.indices.size(); index += 3U) {
            const std::uint32_t i0 = mesh.indices.at(index + 0U);
            const std::uint32_t i1 = mesh.indices.at(index + 1U);
            const std::uint32_t i2 = mesh.indices.at(index + 2U);

            const MeshVertex& v0 = mesh.vertices.at(i0);
            const MeshVertex& v1 = mesh.vertices.at(i1);
            const MeshVertex& v2 = mesh.vertices.at(i2);

            const glm::vec3 edge1 = v1.position - v0.position;
            const glm::vec3 edge2 = v2.position - v0.position;
            const glm::vec2 deltaUV1 = v1.uv - v0.uv;
            const glm::vec2 deltaUV2 = v2.uv - v0.uv;

            const float determinant = (deltaUV1.x * deltaUV2.y) - (deltaUV2.x * deltaUV1.y);
            if(std::fabs(determinant) <= epsilon) {
                continue;
            }

            const float invDet = 1.0F / determinant;
            const glm::vec3 tangent = (edge1 * deltaUV2.y - edge2 * deltaUV1.y) * invDet;
            const glm::vec3 bitangent = (edge2 * deltaUV1.x - edge1 * deltaUV2.x) * invDet;

            tangents.at(i0) += tangent;
            tangents.at(i1) += tangent;
            tangents.at(i2) += tangent;

            bitangents.at(i0) += bitangent;
            bitangents.at(i1) += bitangent;
            bitangents.at(i2) += bitangent;
        }

        for(std::size_t vertexIndex {0U}; vertexIndex < mesh.vertices.size(); ++vertexIndex) {
            MeshVertex& vertex = mesh.vertices.at(vertexIndex);
            const glm::vec3 normal = glm::normalize(vertex.normal);

            glm::vec3 tangent = tangents.at(vertexIndex);
            if(glm::dot(tangent, tangent) <= epsilon) {
                tangent = glm::vec3 {1.0F, 0.0F, 0.0F};
            } else {
                tangent = glm::normalize(tangent - normal * glm::dot(normal, tangent));
            }

            glm::vec3 bitangent = bitangents.at(vertexIndex);
            if(glm::dot(bitangent, bitangent) <= epsilon) {
                bitangent = glm::vec3 {0.0F, 1.0F, 0.0F};
            }

            const float handedness = glm::dot(glm::cross(normal, tangent), bitangent) < 0.0F ? -1.0F : 1.0F;
            vertex.tangent = glm::vec4 {tangent, handedness};
        }
    }
}

namespace vulkano {

Primitive::Primitive(const PrimitiveProperties& properties)
    : m_properties {properties} {
}

Primitive::~Primitive() noexcept = default;

auto Primitive::mesh() const noexcept -> const MeshData& {
    return m_mesh;
}

auto Primitive::properties() noexcept -> PrimitiveProperties& {
    return m_properties;
}

auto Primitive::properties() const noexcept -> const PrimitiveProperties& {
    return m_properties;
}

auto Primitive::is_mesh_dirty() const noexcept -> bool {
    return m_meshDirty;
}

void Primitive::clear_mesh_dirty() noexcept {
    m_meshDirty = false;
}

void Primitive::rebuild_mesh() {
    if(!m_rebuildRequested) {
        return;
    }
    m_mesh = build_mesh();
    m_meshDirty = true;
    m_rebuildRequested = false;
}

auto Primitive::needs_rebuild() const noexcept -> bool {
    return m_rebuildRequested;
}

void Primitive::request_mesh_rebuild() noexcept {
    m_rebuildRequested = true;
}

PlanePrimitive::PlanePrimitive(const PlaneParameters& parameters, const PrimitiveProperties& properties)
    : Primitive {properties}
    , m_parameters {parameters} {
    validate_plane_parameters(m_parameters);
    request_mesh_rebuild();
    rebuild_mesh();
}

auto PlanePrimitive::identifier() const noexcept -> std::string_view {
    return planeIdentifier;
}

auto PlanePrimitive::parameters() const noexcept -> const PlaneParameters& {
    return m_parameters;
}

void PlanePrimitive::set_parameters(const PlaneParameters& parameters) {
    validate_plane_parameters(parameters);
    if(parameters.width == m_parameters.width
        && parameters.depth == m_parameters.depth
        && parameters.uvTiling == m_parameters.uvTiling) {
        return;
    }
    m_parameters = parameters;
    request_mesh_rebuild();
    rebuild_mesh();
}

auto PlanePrimitive::build_mesh() const -> MeshData {
    MeshData mesh {};
    mesh.vertices.resize(4U);
    mesh.indices = {0U, 1U, 2U, 0U, 2U, 3U};

    const float halfWidth = m_parameters.width * 0.5F;
    const float halfDepth = m_parameters.depth * 0.5F;

    mesh.vertices[0].position = glm::vec3 {-halfWidth, 0.0F, -halfDepth};
    mesh.vertices[1].position = glm::vec3 {-halfWidth, 0.0F, halfDepth};
    mesh.vertices[2].position = glm::vec3 {halfWidth, 0.0F, halfDepth};
    mesh.vertices[3].position = glm::vec3 {halfWidth, 0.0F, -halfDepth};

    for(auto& vertex : mesh.vertices) {
        vertex.normal = glm::vec3 {0.0F, 1.0F, 0.0F};
    }

    mesh.vertices[0].uv = glm::vec2 {0.0F, 0.0F};
    mesh.vertices[1].uv = glm::vec2 {0.0F, m_parameters.uvTiling.y};
    mesh.vertices[2].uv = glm::vec2 {m_parameters.uvTiling.x, m_parameters.uvTiling.y};
    mesh.vertices[3].uv = glm::vec2 {m_parameters.uvTiling.x, 0.0F};

    compute_tangents(mesh);
    return mesh;
}

CubePrimitive::CubePrimitive(const PrimitiveProperties& properties)
    : Primitive {properties} {
    request_mesh_rebuild();
    rebuild_mesh();
}

auto CubePrimitive::identifier() const noexcept -> std::string_view {
    return cubeIdentifier;
}

auto CubePrimitive::build_mesh() const -> MeshData {
    MeshData mesh {};
    mesh.vertices.reserve(cubeFaces.size() * faceCornerOffsets.size());
    mesh.indices.reserve(cubeFaces.size() * faceIndicesPattern.size());

    for(const FaceDefinition& face : cubeFaces) {
        const std::uint32_t baseIndex = static_cast<std::uint32_t>(mesh.vertices.size());
        for(std::size_t cornerIndex {0U}; cornerIndex < faceCornerOffsets.size(); ++cornerIndex) {
            const glm::vec2 offset = faceCornerOffsets.at(cornerIndex);
            MeshVertex vertex {};
            vertex.position = (face.normal * defaultCubeHalfExtent)
                + (face.up * offset.x * defaultCubeHalfExtent)
                + (face.right * offset.y * defaultCubeHalfExtent);
            vertex.normal = face.normal;
            vertex.uv = faceCornerUvs.at(cornerIndex);
            mesh.vertices.push_back(vertex);
        }
        for(std::uint32_t indexOffset : faceIndicesPattern) {
            mesh.indices.push_back(baseIndex + indexOffset);
        }
    }

    compute_tangents(mesh);
    return mesh;
}

IcospherePrimitive::IcospherePrimitive(const IcosphereParameters& parameters, const PrimitiveProperties& properties)
    : Primitive {properties}
    , m_parameters {parameters} {
    validate_subdivisions(m_parameters.subdivisions);
    request_mesh_rebuild();
    rebuild_mesh();
}

auto IcospherePrimitive::identifier() const noexcept -> std::string_view {
    return icosphereIdentifier;
}

auto IcospherePrimitive::parameters() const noexcept -> const IcosphereParameters& {
    return m_parameters;
}

void IcospherePrimitive::set_subdivisions(std::uint32_t subdivisions) {
    validate_subdivisions(subdivisions);
    if(subdivisions == m_parameters.subdivisions) {
        return;
    }
    m_parameters.subdivisions = subdivisions;
    request_mesh_rebuild();
    rebuild_mesh();
}

auto IcospherePrimitive::build_mesh() const -> MeshData {
    std::vector<glm::vec3> positions = generate_base_vertices();
    std::vector<std::uint32_t> indices = generate_base_indices();
    subdivide(positions, indices, m_parameters.subdivisions);

    MeshData mesh {};
    mesh.vertices.reserve(positions.size());
    mesh.indices = indices;

    for(const glm::vec3& position : positions) {
        const glm::vec3 normal = glm::normalize(position);
        MeshVertex vertex {};
        vertex.position = normal;
        vertex.normal = normal;
        vertex.uv = compute_uv(normal);
        mesh.vertices.push_back(vertex);
    }

    return mesh;
}

auto IcospherePrimitive::generate_base_vertices() const -> std::vector<glm::vec3> {
    const float goldenRatio = (1.0F + std::sqrt(5.0F)) * 0.5F;

    std::vector<glm::vec3> vertices {
        glm::vec3 {-1.0F, goldenRatio, 0.0F},
        glm::vec3 {1.0F, goldenRatio, 0.0F},
        glm::vec3 {-1.0F, -goldenRatio, 0.0F},
        glm::vec3 {1.0F, -goldenRatio, 0.0F},
        glm::vec3 {0.0F, -1.0F, goldenRatio},
        glm::vec3 {0.0F, 1.0F, goldenRatio},
        glm::vec3 {0.0F, -1.0F, -goldenRatio},
        glm::vec3 {0.0F, 1.0F, -goldenRatio},
        glm::vec3 {goldenRatio, 0.0F, -1.0F},
        glm::vec3 {goldenRatio, 0.0F, 1.0F},
        glm::vec3 {-goldenRatio, 0.0F, -1.0F},
        glm::vec3 {-goldenRatio, 0.0F, 1.0F}
    };

    for(glm::vec3& vertex : vertices) {
        vertex = glm::normalize(vertex);
    }

    return vertices;
}

auto IcospherePrimitive::generate_base_indices() const -> std::vector<std::uint32_t> {
    return {
        0U, 11U, 5U,
        0U, 5U, 1U,
        0U, 1U, 7U,
        0U, 7U, 10U,
        0U, 10U, 11U,
        1U, 5U, 9U,
        5U, 11U, 4U,
        11U, 10U, 2U,
        10U, 7U, 6U,
        7U, 1U, 8U,
        3U, 9U, 4U,
        3U, 4U, 2U,
        3U, 2U, 6U,
        3U, 6U, 8U,
        3U, 8U, 9U,
        4U, 9U, 5U,
        2U, 4U, 11U,
        6U, 2U, 10U,
        8U, 6U, 7U,
        9U, 8U, 1U
    };
}

void IcospherePrimitive::subdivide(
    std::vector<glm::vec3>& vertices,
    std::vector<std::uint32_t>& indices,
    std::uint32_t level) const {
    for(std::uint32_t iteration {0U}; iteration < level; ++iteration) {
        std::unordered_map<std::uint64_t, std::uint32_t> midpointCache {};
        midpointCache.reserve(indices.size());

        std::vector<std::uint32_t> newIndices {};
        newIndices.reserve(indices.size() * 4U);

        for(std::size_t index {0U}; index + 2U < indices.size(); index += 3U) {
            const std::uint32_t i0 = indices.at(index);
            const std::uint32_t i1 = indices.at(index + 1U);
            const std::uint32_t i2 = indices.at(index + 2U);

            const std::uint32_t m0 = midpoint_index(vertices, midpointCache, i0, i1);
            const std::uint32_t m1 = midpoint_index(vertices, midpointCache, i1, i2);
            const std::uint32_t m2 = midpoint_index(vertices, midpointCache, i2, i0);

            newIndices.insert(newIndices.end(), {i0, m0, m2});
            newIndices.insert(newIndices.end(), {i1, m1, m0});
            newIndices.insert(newIndices.end(), {i2, m2, m1});
            newIndices.insert(newIndices.end(), {m0, m1, m2});
        }

        indices = std::move(newIndices);
    }
}

auto IcospherePrimitive::midpoint_index(
    std::vector<glm::vec3>& vertices,
    std::unordered_map<std::uint64_t, std::uint32_t>& cache,
    std::uint32_t first,
    std::uint32_t second) const -> std::uint32_t {
    const std::uint64_t key = edge_key(first, second);
    const auto found = cache.find(key);
    if(found != cache.end()) {
        return found->second;
    }

    glm::vec3 midpoint = glm::normalize((vertices.at(first) + vertices.at(second)) * 0.5F);
    vertices.push_back(midpoint);
    const std::uint32_t index = static_cast<std::uint32_t>(vertices.size() - 1U);
    cache.emplace(key, index);
    return index;
}

auto IcospherePrimitive::compute_uv(const glm::vec3& normal) const noexcept -> glm::vec2 {
    const float twoPi = glm::two_pi<float>();
    const float u = (glm::atan(normal.z, normal.x) / twoPi) + 0.5F;
    const float clampedY = std::clamp(normal.y, -1.0F, 1.0F);
    const float v = (glm::asin(clampedY) / glm::pi<float>()) + 0.5F;
    return glm::vec2 {u, v};
}

} // namespace vulkano
