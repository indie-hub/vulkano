#include <cmath>
#include <numbers>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/geometric.hpp>

#include <vulkan_app/icosphere.hpp>

namespace vulkan_app {
namespace {

using Edge = std::pair<std::uint32_t, std::uint32_t>;

struct EdgeHash final {
    [[nodiscard]] std::size_t operator()(const Edge &e) const noexcept {
        return static_cast<std::size_t>(e.first) << 32U | e.second;
    }
};

std::uint32_t midpoint(std::uint32_t a,
                       std::uint32_t b,
                       std::vector<glm::vec3> &verts,
                       std::unordered_map<Edge, std::uint32_t, EdgeHash> &cache) {
    const Edge key {std::min(a, b), std::max(a, b)};
    const auto it = cache.find(key);
    if (it != cache.end()) {
        return it->second;
    }
    const glm::vec3 mid {glm::normalize((verts[a] + verts[b]) * 0.5F)};
    verts.push_back(mid);
    const std::uint32_t idx {static_cast<std::uint32_t>(verts.size() - 1U)};
    cache[key] = idx;
    return idx;
}

} // namespace

Mesh make_icosphere(const int subdivisions) {
    const float t {(1.F + std::sqrt(5.F)) * 0.5F};
    std::vector<glm::vec3> positions {
        {-1.F,  t,  0.F}, {1.F,  t,  0.F}, {-1.F, -t,  0.F}, {1.F, -t,  0.F},
        {0.F, -1.F,  t}, {0.F,  1.F,  t}, {0.F, -1.F, -t}, {0.F,  1.F, -t},
        { t,  0.F, -1.F}, { t,  0.F,  1.F}, {-t,  0.F, -1.F}, {-t,  0.F,  1.F}
    };
    for (auto &p : positions) {
        p = glm::normalize(p);
    }

    std::vector<std::array<std::uint32_t, 3U>> faces {
        {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
        {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
        {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
        {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
    };

    for (int i {0}; i < subdivisions; ++i) {
        std::unordered_map<Edge, std::uint32_t, EdgeHash> cache {};
        std::vector<std::array<std::uint32_t, 3U>> next {};
        next.reserve(faces.size() * 4U);
        for (const auto &tri : faces) {
            const auto a = midpoint(tri[0], tri[1], positions, cache);
            const auto b = midpoint(tri[1], tri[2], positions, cache);
            const auto c = midpoint(tri[2], tri[0], positions, cache);
            next.push_back({tri[0], a, c});
            next.push_back({tri[1], b, a});
            next.push_back({tri[2], c, b});
            next.push_back({a, b, c});
        }
        faces = std::move(next);
    }

    Mesh mesh {};
    mesh.vertices.resize(positions.size());
    for (std::size_t i {0}; i < positions.size(); ++i) {
        const glm::vec3 n {glm::normalize(positions[i])};
        const float u {std::atan2(n.z, n.x) / (2.F * std::numbers::pi_v<float>) + 0.5F};
        const float v {n.y * 0.5F + 0.5F};
        const glm::vec3 tangent {glm::normalize(glm::vec3 {-n.z, 0.F, n.x})};
        const glm::vec3 bitangent {glm::normalize(glm::cross(n, tangent))};
        mesh.vertices[i] = Vertex {positions[i], n, {u, v}, tangent, bitangent};
    }

    mesh.indices.reserve(faces.size() * 3U);
    for (const auto &tri : faces) {
        mesh.indices.push_back(tri[0]);
        mesh.indices.push_back(tri[1]);
        mesh.indices.push_back(tri[2]);
    }

    return mesh;
}

} // namespace vulkan_app
