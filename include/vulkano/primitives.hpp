#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <vulkano/mesh.hpp>

namespace vulkano {

class Primitive {
public:
    explicit Primitive(const PrimitiveProperties& properties = {});
    virtual ~Primitive() noexcept;

    [[nodiscard]] auto mesh() const noexcept -> const MeshData&;
    [[nodiscard]] auto properties() noexcept -> PrimitiveProperties&;
    [[nodiscard]] auto properties() const noexcept -> const PrimitiveProperties&;
    [[nodiscard]] auto is_mesh_dirty() const noexcept -> bool;
    void clear_mesh_dirty() noexcept;
    void rebuild_mesh();
    [[nodiscard]] auto needs_rebuild() const noexcept -> bool;

    [[nodiscard]] virtual auto identifier() const noexcept -> std::string_view = 0;

protected:
    void request_mesh_rebuild() noexcept;
    [[nodiscard]] virtual auto build_mesh() const -> MeshData = 0;

private:
    PrimitiveProperties m_properties {};
    MeshData m_mesh {};
    bool m_meshDirty {false};
    bool m_rebuildRequested {true};
};

class PlanePrimitive final : public Primitive {
public:
    explicit PlanePrimitive(const PlaneParameters& parameters = {}, const PrimitiveProperties& properties = {});

    [[nodiscard]] auto identifier() const noexcept -> std::string_view override;
    [[nodiscard]] auto parameters() const noexcept -> const PlaneParameters&;
    void set_parameters(const PlaneParameters& parameters);

protected:
    [[nodiscard]] auto build_mesh() const -> MeshData override;

private:
    PlaneParameters m_parameters {};
};

class CubePrimitive final : public Primitive {
public:
    explicit CubePrimitive(const PrimitiveProperties& properties = {});

    [[nodiscard]] auto identifier() const noexcept -> std::string_view override;

protected:
    [[nodiscard]] auto build_mesh() const -> MeshData override;
};

class IcospherePrimitive final : public Primitive {
public:
    explicit IcospherePrimitive(const IcosphereParameters& parameters = {}, const PrimitiveProperties& properties = {});

    [[nodiscard]] auto identifier() const noexcept -> std::string_view override;
    [[nodiscard]] auto parameters() const noexcept -> const IcosphereParameters&;
    void set_subdivisions(std::uint32_t subdivisions);

protected:
    [[nodiscard]] auto build_mesh() const -> MeshData override;

private:
    [[nodiscard]] auto generate_base_vertices() const -> std::vector<glm::vec3>;
    [[nodiscard]] auto generate_base_indices() const -> std::vector<std::uint32_t>;
    void subdivide(std::vector<glm::vec3>& vertices, std::vector<std::uint32_t>& indices, std::uint32_t level) const;
    [[nodiscard]] auto midpoint_index(
        std::vector<glm::vec3>& vertices,
        std::unordered_map<std::uint64_t, std::uint32_t>& cache,
        std::uint32_t first,
        std::uint32_t second) const -> std::uint32_t;
    [[nodiscard]] auto compute_uv(const glm::vec3& normal) const noexcept -> glm::vec2;

    IcosphereParameters m_parameters {};
};

} // namespace vulkano

