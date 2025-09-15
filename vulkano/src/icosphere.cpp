#include <vulkano/icosphere.hpp>

#include <array>
#include <unordered_map>
#include <glm/gtc/constants.hpp>

namespace vulkano {

namespace {
struct EdgeKey final {
    std::uint32_t a {};
    std::uint32_t b {};
    bool operator==(const EdgeKey& other) const noexcept
    {
        return a == other.a && b == other.b;
    }
};

struct EdgeKeyHash final {
    std::size_t operator()(const EdgeKey& k) const noexcept
    {
        return (static_cast<std::size_t>(k.a) << 32) ^ static_cast<std::size_t>(k.b);
    }
};

glm::vec3 normalize_safe(glm::vec3 v) noexcept
{
    const float len = glm::length(v);
    if (len > 0.0f) {
        return v / len;
    } else {
        return glm::vec3 {0.0f, 0.0f, 0.0f};
    }
}

glm::vec2 spherical_uv(const glm::vec3 p) noexcept
{
    const float u = 0.5f + std::atan2(p.z, p.x) / (2.0f * glm::pi<float>());
    const float v = 0.5f - std::asin(glm::clamp(p.y, -1.0f, 1.0f)) / glm::pi<float>();
    return glm::vec2 {u, v};
}
} // namespace

Mesh make_icosphere(const int subdivisions)
{
    const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;

    std::vector<Vertex> vertices {};
    vertices.reserve(12);
    auto add_vertex = [&](glm::vec3 p) -> std::uint32_t {
        p = normalize_safe(p);
        Vertex v {};
        v.position = p;
        v.normal = p;
        v.uv = spherical_uv(p);
        vertices.push_back(v);
        return static_cast<std::uint32_t>(vertices.size() - 1);
    };

    add_vertex({-1, t, 0});
    add_vertex({1, t, 0});
    add_vertex({-1, -t, 0});
    add_vertex({1, -t, 0});
    add_vertex({0, -1, t});
    add_vertex({0, 1, t});
    add_vertex({0, -1, -t});
    add_vertex({0, 1, -t});
    add_vertex({t, 0, -1});
    add_vertex({t, 0, 1});
    add_vertex({-t, 0, -1});
    add_vertex({-t, 0, 1});

    std::vector<std::uint32_t> indices = {
        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4,
        11, 10, 2, 10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8,
        3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1};

    std::unordered_map<EdgeKey, std::uint32_t, EdgeKeyHash> midpoint_cache {};
    midpoint_cache.reserve(1024);

    auto midpoint = [&](std::uint32_t a, std::uint32_t b) -> std::uint32_t {
        EdgeKey key {std::min(a, b), std::max(a, b)};
        const auto it = midpoint_cache.find(key);
        if (it != midpoint_cache.end()) {
            return it->second;
        }
        const glm::vec3 p = normalize_safe((vertices[a].position + vertices[b].position) * 0.5f);
        const std::uint32_t idx = static_cast<std::uint32_t>(vertices.size());
        Vertex v {};
        v.position = p;
        v.normal = p;
        v.uv = spherical_uv(p);
        vertices.push_back(v);
        midpoint_cache.emplace(key, idx);
        return idx;
    };

    for (int s = 0; s < subdivisions; ++s) {
        std::vector<std::uint32_t> next_indices {};
        next_indices.reserve(indices.size() * 4);
        for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
            const auto i0 = indices[i + 0];
            const auto i1 = indices[i + 1];
            const auto i2 = indices[i + 2];
            const auto a = midpoint(i0, i1);
            const auto b = midpoint(i1, i2);
            const auto c = midpoint(i2, i0);
            next_indices.insert(next_indices.end(), {i0, a, c, i1, b, a, i2, c, b, a, b, c});
        }
        indices = std::move(next_indices);
        midpoint_cache.clear();
    }

    // Fix UV seam: duplicate vertices with u near 0/1
    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        const std::uint32_t tri[3] {indices[i], indices[i + 1], indices[i + 2]};
        float umin = 1.0f, umax = 0.0f;
        for (int k = 0; k < 3; ++k) {
            umin = std::min(umin, vertices[tri[k]].uv.x);
            umax = std::max(umax, vertices[tri[k]].uv.x);
        }
        if (umax - umin > 0.5f) {
            for (int k = 0; k < 3; ++k) {
                const auto idx = tri[k];
                if (vertices[idx].uv.x < 0.5f) {
                    Vertex v = vertices[idx];
                    v.uv.x += 1.0f;
                    indices[i + k] = static_cast<std::uint32_t>(vertices.size());
                    vertices.push_back(v);
                }
            }
        }
    }

    compute_tangents(vertices, indices);
    return Mesh {std::move(vertices), std::move(indices)};
}

} // namespace vulkano

