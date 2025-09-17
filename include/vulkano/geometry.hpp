// geometry.hpp
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace vulkano {

struct Vertex final {
    glm::vec3 position {};
    glm::vec3 normal {};
    glm::vec2 uv {};
    glm::vec3 tangent {};
};

struct Material final {
    glm::vec3 baseColor {1.0F, 1.0F, 1.0F};
    float shininess {32.0F};
    // Texture controls
    bool useAlbedo {true};
    bool useNormal {true};
    float normalStrength {1.0F};
};

struct Transform final {
    glm::vec3 position {0.0F, 0.0F, 0.0F};
    glm::vec3 rotationEuler {0.0F, 0.0F, 0.0F};
    glm::vec3 scale {1.0F, 1.0F, 1.0F};
};

class Primitive {
public:
    virtual ~Primitive() = default;

    [[nodiscard]] virtual const std::vector<Vertex>& vertices() const noexcept = 0;
    [[nodiscard]] virtual const std::vector<std::uint32_t>& indices() const noexcept = 0;
    [[nodiscard]] virtual const char* type_name() const noexcept = 0;

    [[nodiscard]] const Transform& transform() const noexcept;
    [[nodiscard]] const Material& material() const noexcept;
    void set_transform(const Transform& t) noexcept;
    void set_material(const Material& m) noexcept;

protected:
    Transform transform_ {};
    Material material_ {};
};

class Plane final : public Primitive {
public:
    struct Params final {
        float width {1.0F};
        float depth {1.0F};
        glm::vec2 uvTiling {1.0F, 1.0F};
    };

    Plane() noexcept;
    explicit Plane(const Params& p) noexcept;

    [[nodiscard]] const std::vector<Vertex>& vertices() const noexcept override;
    [[nodiscard]] const std::vector<std::uint32_t>& indices() const noexcept override;
    [[nodiscard]] const char* type_name() const noexcept override;

    // UI support: update/get UV tiling and rebuild geometry as needed
    void set_uv_tiling(const glm::vec2& tiling) noexcept;
    [[nodiscard]] glm::vec2 uv_tiling() const noexcept;

private:
    void build() noexcept;

private:
    Params params_ {};
    std::vector<Vertex> vertices_ {};
    std::vector<std::uint32_t> indices_ {};
};

class Cube final : public Primitive {
public:
    explicit Cube() noexcept;

    [[nodiscard]] const std::vector<Vertex>& vertices() const noexcept override;
    [[nodiscard]] const std::vector<std::uint32_t>& indices() const noexcept override;
    [[nodiscard]] const char* type_name() const noexcept override;

private:
    void build() noexcept;

private:
    std::vector<Vertex> vertices_ {};
    std::vector<std::uint32_t> indices_ {};
};

class Icosphere final : public Primitive {
public:
    struct Params final {
        std::uint32_t subdivisions {0U}; // 0..5 suggested
    };

    Icosphere() noexcept;
    explicit Icosphere(const Params& p) noexcept;

    [[nodiscard]] const std::vector<Vertex>& vertices() const noexcept override;
    [[nodiscard]] const std::vector<std::uint32_t>& indices() const noexcept override;
    [[nodiscard]] const char* type_name() const noexcept override;

    void set_subdivisions(std::uint32_t subdivisions) noexcept;
    [[nodiscard]] std::uint32_t subdivisions() const noexcept;

private:
    void build_icosahedron() noexcept;
    void subdivide_once() noexcept;
    static std::uint64_t edge_key(std::uint32_t a, std::uint32_t b) noexcept;
    std::uint32_t midpoint(std::uint32_t a, std::uint32_t b) noexcept;
    void normalize_and_uv(std::uint32_t idx) noexcept;

private:
    Params params_ {};
    std::vector<Vertex> vertices_ {};
    std::vector<std::uint32_t> indices_ {};
    // cache for midpoint generation
    std::vector<std::pair<std::uint64_t, std::uint32_t>> midpoint_cache_ {};
};

} // namespace vulkano
