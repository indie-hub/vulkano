#include <vulkano/icosphere.hpp>
#include <vulkano/mesh.hpp>

#include <array>
#include <cmath>
#include <unordered_map>

namespace vulkano {

namespace {

struct Vec3 final {
    float x {};
    float y {};
    float z {};
};

struct Vec2 final {
    float u {};
    float v {};
};

static inline Vec3 normalize(const Vec3 v) noexcept {
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len == 0.0F) {
        return Vec3 {0.0F, 0.0F, 0.0F};
    }
    const float inv = 1.0F / len;
    return Vec3 {v.x * inv, v.y * inv, v.z * inv};
}

static inline Vec3 add(const Vec3 a, const Vec3 b) noexcept {
    return Vec3 {a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline Vec3 sub(const Vec3 a, const Vec3 b) noexcept {
    return Vec3 {a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline Vec3 mul(const Vec3 a, const float s) noexcept {
    return Vec3 {a.x * s, a.y * s, a.z * s};
}

static inline Vec3 cross(const Vec3 a, const Vec3 b) noexcept {
    return Vec3 {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

static inline float dot(const Vec3 a, const Vec3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline Vec2 compute_uv(const Vec3 n) noexcept {
    // Longitude/latitude mapping
    const float u = 0.5F + std::atan2(n.z, n.x) / (2.0F * static_cast<float>(M_PI));
    const float v = 0.5F - std::asin(n.y) / static_cast<float>(M_PI);
    return Vec2 {u, v};
}

struct Key final {
    std::uint32_t a {};
    std::uint32_t b {};
};

struct KeyHash final {
    std::size_t operator()(const Key& k) const noexcept {
        return (static_cast<std::size_t>(k.a) << 32U) ^ static_cast<std::size_t>(k.b);
    }
};

struct KeyEq final {
    bool operator()(const Key& x, const Key& y) const noexcept {
        return (x.a == y.a && x.b == y.b) || (x.a == y.b && x.b == y.a);
    }
};

} // namespace

std::unique_ptr<Mesh> IcosphereBuilder::build(const std::uint32_t subdivisions) const {
    auto mesh = std::make_unique<Mesh>();
    mesh->vertices.clear();
    mesh->indices.clear();

    // Icosahedron vertices
    const float t = (1.0F + std::sqrt(5.0F)) * 0.5F;
    std::vector<Vec3> positions {
        Vec3 {-1.0F, t, 0.0F},   Vec3 {1.0F, t, 0.0F},    Vec3 {-1.0F, -t, 0.0F},  Vec3 {1.0F, -t, 0.0F},
        Vec3 {0.0F, -1.0F, t},   Vec3 {0.0F, 1.0F, t},    Vec3 {0.0F, -1.0F, -t},  Vec3 {0.0F, 1.0F, -t},
        Vec3 {t, 0.0F, -1.0F},   Vec3 {t, 0.0F, 1.0F},    Vec3 {-t, 0.0F, -1.0F},  Vec3 {-t, 0.0F, 1.0F},
    };

    for (auto& p : positions) {
        p = normalize(p);
    }

    std::vector<std::uint32_t> faces {
        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2,
        10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4,
        11, 6, 2, 10, 8, 6, 7, 9, 8, 1,
    };

    auto midpoint = [&](std::uint32_t a, std::uint32_t b, std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq>& cache) {
        const Key key {a < b ? a : b, a < b ? b : a};
        const auto it = cache.find(key);
        if (it != cache.end()) {
            return it->second;
        }
        const Vec3 mid = normalize(mul(add(positions[a], positions[b]), 0.5F));
        positions.push_back(mid);
        const std::uint32_t index = static_cast<std::uint32_t>(positions.size() - 1);
        cache.emplace(key, index);
        return index;
    };

    for (std::uint32_t i = 0; i < subdivisions; ++i) {
        std::unordered_map<Key, std::uint32_t, KeyHash, KeyEq> cache {};
        std::vector<std::uint32_t> new_faces {};
        new_faces.reserve(faces.size() * 4U);
        for (std::size_t f = 0; f < faces.size(); f += 3) {
            const std::uint32_t i0 = faces[f + 0];
            const std::uint32_t i1 = faces[f + 1];
            const std::uint32_t i2 = faces[f + 2];
            const std::uint32_t m0 = midpoint(i0, i1, cache);
            const std::uint32_t m1 = midpoint(i1, i2, cache);
            const std::uint32_t m2 = midpoint(i2, i0, cache);
            // Create 4 new faces
            new_faces.insert(new_faces.end(), {i0, m0, m2, i1, m1, m0, i2, m2, m1, m0, m1, m2});
        }
        faces.swap(new_faces);
    }

    // Convert to Mesh with normals/tangents/bitangents and UVs
    mesh->vertices.resize(positions.size());
    for (std::size_t i = 0; i < positions.size(); ++i) {
        const Vec3 n = normalize(positions[i]);
        const Vec2 uv = compute_uv(n);
        Vertex v {};
        v.position[0] = n.x;
        v.position[1] = n.y;
        v.position[2] = n.z;
        v.normal[0] = n.x;
        v.normal[1] = n.y;
        v.normal[2] = n.z;
        v.tangent[0] = 1.0F;
        v.tangent[1] = 0.0F;
        v.tangent[2] = 0.0F;
        v.bitangent[0] = 0.0F;
        v.bitangent[1] = 1.0F;
        v.bitangent[2] = 0.0F;
        v.texcoord[0] = uv.u;
        v.texcoord[1] = uv.v;
        mesh->vertices[i] = v;
    }

    mesh->indices = faces;

    // Compute tangents/bitangents per triangle (simple accumulation)
    std::vector<Vec3> tan1(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
    std::vector<Vec3> tan2(mesh->vertices.size(), Vec3 {0.0F, 0.0F, 0.0F});
    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
        const std::uint32_t i0 = mesh->indices[i + 0];
        const std::uint32_t i1 = mesh->indices[i + 1];
        const std::uint32_t i2 = mesh->indices[i + 2];

        const Vertex& v0 = mesh->vertices[i0];
        const Vertex& v1 = mesh->vertices[i1];
        const Vertex& v2 = mesh->vertices[i2];

        const Vec3 p0 {v0.position[0], v0.position[1], v0.position[2]};
        const Vec3 p1 {v1.position[0], v1.position[1], v1.position[2]};
        const Vec3 p2 {v2.position[0], v2.position[1], v2.position[2]};

        const Vec2 w0 {v0.texcoord[0], v0.texcoord[1]};
        const Vec2 w1 {v1.texcoord[0], v1.texcoord[1]};
        const Vec2 w2 {v2.texcoord[0], v2.texcoord[1]};

        const float x1 = p1.x - p0.x;
        const float x2 = p2.x - p0.x;
        const float y1 = p1.y - p0.y;
        const float y2 = p2.y - p0.y;
        const float z1 = p1.z - p0.z;
        const float z2 = p2.z - p0.z;

        const float s1 = w1.u - w0.u;
        const float s2 = w2.u - w0.u;
        const float t1 = w1.v - w0.v;
        const float t2 = w2.v - w0.v;

        const float r = (s1 * t2 - s2 * t1);
        const float inv = (std::fabs(r) < 1e-6F) ? 1.0F : (1.0F / r);
        const Vec3 sdir {(t2 * x1 - t1 * x2) * inv, (t2 * y1 - t1 * y2) * inv, (t2 * z1 - t1 * z2) * inv};
        const Vec3 tdir {(s1 * x2 - s2 * x1) * inv, (s1 * y2 - s2 * y1) * inv, (s1 * z2 - s2 * z1) * inv};

        tan1[i0] = add(tan1[i0], sdir);
        tan1[i1] = add(tan1[i1], sdir);
        tan1[i2] = add(tan1[i2], sdir);
        tan2[i0] = add(tan2[i0], tdir);
        tan2[i1] = add(tan2[i1], tdir);
        tan2[i2] = add(tan2[i2], tdir);
    }

    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
        const Vec3 n {mesh->vertices[i].normal[0], mesh->vertices[i].normal[1], mesh->vertices[i].normal[2]};
        Vec3 t = tan1[i];
        // Gram-Schmidt orthogonalize
        t = sub(t, mul(n, dot(n, t)));
        t = normalize(t);
        const Vec3 b = cross(n, t);
        mesh->vertices[i].tangent[0] = t.x;
        mesh->vertices[i].tangent[1] = t.y;
        mesh->vertices[i].tangent[2] = t.z;
        mesh->vertices[i].bitangent[0] = b.x;
        mesh->vertices[i].bitangent[1] = b.y;
        mesh->vertices[i].bitangent[2] = b.z;
    }

    return mesh;
}

} // namespace vulkano

