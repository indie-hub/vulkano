#include <vulkano/geometry.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace vulkano {

namespace {
    constexpr float pi() noexcept { return 3.14159265358979323846F; }
    constexpr float two_pi() noexcept { return 2.0F * pi(); }

    [[nodiscard]] constexpr glm::vec3 lerp3(const glm::vec3& a, const glm::vec3& b, float t) noexcept {
        return glm::vec3 {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
    }

    [[nodiscard]] constexpr std::uint64_t morton64(std::uint32_t a, std::uint32_t b) noexcept {
        const std::uint64_t lo {static_cast<std::uint64_t>(std::min(a, b))};
        const std::uint64_t hi {static_cast<std::uint64_t>(std::max(a, b))};
        return (hi << 32U) | lo;
    }
}

// ---- Primitive common methods (out-of-line to keep headers free of impl) ----
const Transform& Primitive::transform() const noexcept {
    return transform_;
}

const Material& Primitive::material() const noexcept {
    return material_;
}

void Primitive::set_transform(const Transform& t) noexcept {
    transform_ = t;
}

void Primitive::set_material(const Material& m) noexcept {
    material_ = m;
}

Plane::Plane() noexcept : params_ {} {
    build();
}

Plane::Plane(const Params& p) noexcept : params_ {p} {
    build();
}

void Plane::build() noexcept {
    vertices_.clear();
    indices_.clear();

    const float halfW {params_.width * 0.5F};
    const float halfD {params_.depth * 0.5F};
    const glm::vec3 n {0.0F, 1.0F, 0.0F};
    const glm::vec2 uvScale {params_.uvTiling};

    // XZ plane at y = 0, CCW winding
    vertices_.push_back(Vertex {glm::vec3 {-halfW, 0.0F, -halfD}, n, glm::vec2 {0.0F, 0.0F} * uvScale});
    vertices_.push_back(Vertex {glm::vec3 {+halfW, 0.0F, -halfD}, n, glm::vec2 {1.0F, 0.0F} * uvScale});
    vertices_.push_back(Vertex {glm::vec3 {+halfW, 0.0F, +halfD}, n, glm::vec2 {1.0F, 1.0F} * uvScale});
    vertices_.push_back(Vertex {glm::vec3 {-halfW, 0.0F, +halfD}, n, glm::vec2 {0.0F, 1.0F} * uvScale});

    indices_.push_back(0U);
    indices_.push_back(1U);
    indices_.push_back(2U);
    indices_.push_back(2U);
    indices_.push_back(3U);
    indices_.push_back(0U);
}

const std::vector<Vertex>& Plane::vertices() const noexcept {
    return vertices_;
}

const std::vector<std::uint32_t>& Plane::indices() const noexcept {
    return indices_;
}

const char* Plane::type_name() const noexcept {
    return "Plane";
}

Cube::Cube() noexcept {
    build();
}

void Cube::build() noexcept {
    vertices_.clear();
    indices_.clear();

    // 6 faces, 4 verts per face (unique normals/uvs)
    struct FaceDesc final {
        glm::vec3 n {};
        glm::vec3 u {};
        glm::vec3 v {};
        glm::vec3 center {};
    };

    constexpr float h {0.5F};
    const std::array<FaceDesc, 6> faces {
        FaceDesc {glm::vec3 {0.0F, 0.0F, 1.0F}, glm::vec3 {1.0F, 0.0F, 0.0F}, glm::vec3 {0.0F, 1.0F, 0.0F}, glm::vec3 {0.0F, 0.0F, h}},
        FaceDesc {glm::vec3 {0.0F, 0.0F, -1.0F}, glm::vec3 {-1.0F, 0.0F, 0.0F}, glm::vec3 {0.0F, 1.0F, 0.0F}, glm::vec3 {0.0F, 0.0F, -h}},
        FaceDesc {glm::vec3 {1.0F, 0.0F, 0.0F}, glm::vec3 {0.0F, 0.0F, -1.0F}, glm::vec3 {0.0F, 1.0F, 0.0F}, glm::vec3 {h, 0.0F, 0.0F}},
        FaceDesc {glm::vec3 {-1.0F, 0.0F, 0.0F}, glm::vec3 {0.0F, 0.0F, 1.0F}, glm::vec3 {0.0F, 1.0F, 0.0F}, glm::vec3 {-h, 0.0F, 0.0F}},
        FaceDesc {glm::vec3 {0.0F, 1.0F, 0.0F}, glm::vec3 {1.0F, 0.0F, 0.0F}, glm::vec3 {0.0F, 0.0F, -1.0F}, glm::vec3 {0.0F, h, 0.0F}},
        FaceDesc {glm::vec3 {0.0F, -1.0F, 0.0F}, glm::vec3 {1.0F, 0.0F, 0.0F}, glm::vec3 {0.0F, 0.0F, 1.0F}, glm::vec3 {0.0F, -h, 0.0F}},
    };

    const glm::vec2 uv00 {0.0F, 0.0F};
    const glm::vec2 uv10 {1.0F, 0.0F};
    const glm::vec2 uv11 {1.0F, 1.0F};
    const glm::vec2 uv01 {0.0F, 1.0F};

    for (const auto& f : faces) {
        const glm::vec3 p0 {f.center - f.u * h - f.v * h};
        const glm::vec3 p1 {f.center + f.u * h - f.v * h};
        const glm::vec3 p2 {f.center + f.u * h + f.v * h};
        const glm::vec3 p3 {f.center - f.u * h + f.v * h};

        const std::uint32_t base {static_cast<std::uint32_t>(vertices_.size())};
        vertices_.push_back(Vertex {p0, f.n, uv00});
        vertices_.push_back(Vertex {p1, f.n, uv10});
        vertices_.push_back(Vertex {p2, f.n, uv11});
        vertices_.push_back(Vertex {p3, f.n, uv01});

        indices_.push_back(base + 0U);
        indices_.push_back(base + 1U);
        indices_.push_back(base + 2U);
        indices_.push_back(base + 2U);
        indices_.push_back(base + 3U);
        indices_.push_back(base + 0U);
    }
}

const std::vector<Vertex>& Cube::vertices() const noexcept {
    return vertices_;
}

const std::vector<std::uint32_t>& Cube::indices() const noexcept {
    return indices_;
}

const char* Cube::type_name() const noexcept {
    return "Cube";
}

Icosphere::Icosphere() noexcept : params_ {} {
    build_icosahedron();
    for (std::uint32_t i {0U}; i < params_.subdivisions; ++i) {
        subdivide_once();
    }
    for (std::uint32_t i {0U}; i < static_cast<std::uint32_t>(vertices_.size()); ++i) {
        normalize_and_uv(i);
    }
}

Icosphere::Icosphere(const Params& p) noexcept : params_ {p} {
    if (params_.subdivisions > 8U) {
        params_.subdivisions = 8U; // guard
    }
    build_icosahedron();
    for (std::uint32_t i {0U}; i < params_.subdivisions; ++i) {
        subdivide_once();
    }
    // Normalize/UV all vertices (in case of 0 subdivisions)
    for (std::uint32_t i {0U}; i < static_cast<std::uint32_t>(vertices_.size()); ++i) {
        normalize_and_uv(i);
    }
}

void Icosphere::set_subdivisions(std::uint32_t subdivisions) noexcept {
    const std::uint32_t clamped {subdivisions > 8U ? 8U : subdivisions};
    if (params_.subdivisions == clamped) {
        return;
    }
    params_.subdivisions = clamped;
    build_icosahedron();
    for (std::uint32_t i {0U}; i < params_.subdivisions; ++i) {
        subdivide_once();
    }
    for (std::uint32_t i {0U}; i < static_cast<std::uint32_t>(vertices_.size()); ++i) {
        normalize_and_uv(i);
    }
}

std::uint32_t Icosphere::subdivisions() const noexcept {
    return params_.subdivisions;
}

void Icosphere::build_icosahedron() noexcept {
    vertices_.clear();
    indices_.clear();
    midpoint_cache_.clear();

    // Create unit icosahedron
    const float t {(1.0F + std::sqrt(5.0F)) * 0.5F};
    // Normalize factor to approximate unit sphere along axes
    const float invLen {1.0F / std::sqrt(1.0F + t * t)};
    const float a {invLen};
    const float b {t * invLen};

    const std::array<glm::vec3, 12> verts {
        glm::vec3 {-a,  b, 0.0F}, glm::vec3 { a,  b, 0.0F}, glm::vec3 {-a, -b, 0.0F}, glm::vec3 { a, -b, 0.0F},
        glm::vec3 {0.0F, -a,  b}, glm::vec3 {0.0F,  a,  b}, glm::vec3 {0.0F, -a, -b}, glm::vec3 {0.0F,  a, -b},
        glm::vec3 { b, 0.0F, -a}, glm::vec3 { b, 0.0F,  a}, glm::vec3 {-b, 0.0F, -a}, glm::vec3 {-b, 0.0F,  a}
    };

    vertices_.reserve(verts.size());
    for (const auto& p : verts) {
        vertices_.push_back(Vertex {p, glm::vec3 {0.0F, 0.0F, 0.0F}, glm::vec2 {0.0F, 0.0F}});
    }

    // 20 faces
    const std::array<std::uint32_t, 60> idx {
        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7,10, 0,10,11,
        1, 5, 9, 5, 11, 4, 11,10, 2,10, 7, 6, 7, 1, 8,
        3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9,
        4, 9, 5, 2, 4,11, 6, 2,10, 8, 6, 7, 9, 8, 1
    };
    indices_.assign(idx.begin(), idx.end());
}

std::uint64_t Icosphere::edge_key(std::uint32_t a, std::uint32_t b) noexcept {
    return morton64(a, b);
}

std::uint32_t Icosphere::midpoint(std::uint32_t a, std::uint32_t b) noexcept {
    const std::uint64_t key {edge_key(a, b)};
    for (const auto& kv : midpoint_cache_) {
        if (kv.first == key) {
            return kv.second;
        }
    }
    const glm::vec3 pa {vertices_[a].position};
    const glm::vec3 pb {vertices_[b].position};
    const glm::vec3 pm {lerp3(pa, pb, 0.5F)};
    const std::uint32_t idx {static_cast<std::uint32_t>(vertices_.size())};
    vertices_.push_back(Vertex {pm, glm::vec3 {0.0F, 0.0F, 0.0F}, glm::vec2 {0.0F, 0.0F}});
    midpoint_cache_.emplace_back(key, idx);
    return idx;
}

void Icosphere::normalize_and_uv(std::uint32_t idx) noexcept {
    glm::vec3 p {vertices_[idx].position};
    const float len {std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z)};
    const float inv {len > std::numeric_limits<float>::epsilon() ? 1.0F / len : 1.0F};
    p = glm::vec3 {p.x * inv, p.y * inv, p.z * inv};
    vertices_[idx].position = p;
    vertices_[idx].normal = p;
    const float u {0.5F + (std::atan2(p.z, p.x) / two_pi())};
    const float v {std::acos(std::clamp(p.y, -1.0F, 1.0F)) / pi()};
    vertices_[idx].uv = glm::vec2 {u, v};
}

void Icosphere::subdivide_once() noexcept {
    std::vector<std::uint32_t> newIdx {};
    newIdx.reserve(indices_.size() * 4U);
    midpoint_cache_.clear();
    for (std::size_t i {0U}; i + 2U < indices_.size(); i += 3U) {
        const std::uint32_t i0 {indices_[i + 0U]};
        const std::uint32_t i1 {indices_[i + 1U]};
        const std::uint32_t i2 {indices_[i + 2U]};
        const std::uint32_t a {midpoint(i0, i1)};
        const std::uint32_t b {midpoint(i1, i2)};
        const std::uint32_t c {midpoint(i2, i0)};
        // 4 new tris
        newIdx.push_back(i0); newIdx.push_back(a);  newIdx.push_back(c);
        newIdx.push_back(i1); newIdx.push_back(b);  newIdx.push_back(a);
        newIdx.push_back(i2); newIdx.push_back(c);  newIdx.push_back(b);
        newIdx.push_back(a);  newIdx.push_back(b);  newIdx.push_back(c);
    }
    indices_.swap(newIdx);
}

const std::vector<Vertex>& Icosphere::vertices() const noexcept {
    return vertices_;
}

const std::vector<std::uint32_t>& Icosphere::indices() const noexcept {
    return indices_;
}

const char* Icosphere::type_name() const noexcept {
    return "Icosphere";
}

} // namespace vulkano
