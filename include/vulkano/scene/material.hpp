#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

namespace vulkano::scene {
struct MaterialId final {
    std::uint32_t value {0U};

    [[nodiscard]] constexpr bool operator==(const MaterialId&) const noexcept = default;
    [[nodiscard]] static constexpr MaterialId invalid() noexcept {
        return MaterialId {std::numeric_limits<std::uint32_t>::max()};
    }
};

struct MaterialProperties final {
    glm::vec3 baseColor {1.0F, 1.0F, 1.0F};
    float metallic {0.0F};
    float roughness {0.5F};
    float ambientOcclusion {1.0F};
    float normalStrength {1.0F};
    glm::vec3 emissive {0.0F, 0.0F, 0.0F};
    float emissiveIntensity {0.0F};
};

struct MaterialTextures final {
    std::string baseColorPath {};
    std::string normalPath {};
    std::string metallicRoughnessPath {};
    std::string ambientOcclusionPath {};
};

struct Material final {
    MaterialProperties properties {};
    MaterialTextures textures {};
    bool useBaseColorTexture {false};
    bool useNormalTexture {false};
    bool useMetallicRoughnessTexture {false};
    bool useAmbientOcclusionTexture {false};
};

struct MaterialTextureHandles final {
    std::uint32_t baseColor {std::numeric_limits<std::uint32_t>::max()};
    std::uint32_t normal {std::numeric_limits<std::uint32_t>::max()};
    std::uint32_t metallicRoughness {std::numeric_limits<std::uint32_t>::max()};
    std::uint32_t ambientOcclusion {std::numeric_limits<std::uint32_t>::max()};

    [[nodiscard]] constexpr bool valid_base_color() const noexcept {
        return baseColor != std::numeric_limits<std::uint32_t>::max();
    }
};

class MaterialRegistry final {
public:
    MaterialRegistry();

    [[nodiscard]] MaterialId add_material(const Material& material);
    void update_material(MaterialId id, const Material& material);

    [[nodiscard]] const Material& material(MaterialId id) const;
    [[nodiscard]] Material& material(MaterialId id);
    [[nodiscard]] const Material& default_material() const noexcept;
    [[nodiscard]] MaterialId default_material_id() const noexcept;
    [[nodiscard]] bool contains(MaterialId id) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const std::vector<Material>& materials() const noexcept;

private:
    [[nodiscard]] bool is_valid(MaterialId id) const noexcept;

    std::vector<Material> m_materials {};
    MaterialId m_defaultId {};
};
} // namespace vulkano::scene
