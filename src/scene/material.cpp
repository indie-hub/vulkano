#include <vulkano/scene/material.hpp>

#include <stdexcept>

namespace vulkano::scene {
namespace {
[[nodiscard]] Material make_default_material() noexcept {
    Material defaultMaterial {};
    defaultMaterial.properties.baseColor = glm::vec3 {0.8F, 0.8F, 0.8F};
    defaultMaterial.properties.metallic = 0.0F;
    defaultMaterial.properties.roughness = 0.5F;
    defaultMaterial.properties.ambientOcclusion = 1.0F;
    return defaultMaterial;
}
} // namespace

MaterialRegistry::MaterialRegistry()
    : m_materials {}
    , m_defaultId {0U} {
    m_materials.push_back(make_default_material());
}

MaterialId MaterialRegistry::add_material(const Material& material) {
    m_materials.push_back(material);
    const std::size_t index {m_materials.size() - 1U};
    return MaterialId {static_cast<std::uint32_t>(index)};
}

void MaterialRegistry::update_material(MaterialId id, const Material& material) {
    if (!is_valid(id)) {
        throw std::out_of_range {"MaterialRegistry::update_material received invalid material id"};
    }
    m_materials[id.value] = material;
}

const Material& MaterialRegistry::material(MaterialId id) const {
    if (!is_valid(id)) {
        throw std::out_of_range {"MaterialRegistry::material received invalid material id"};
    }
    return m_materials[id.value];
}

Material& MaterialRegistry::material(MaterialId id) {
    if (!is_valid(id)) {
        throw std::out_of_range {"MaterialRegistry::material received invalid material id"};
    }
    return m_materials[id.value];
}

const Material& MaterialRegistry::default_material() const noexcept {
    return m_materials[m_defaultId.value];
}

MaterialId MaterialRegistry::default_material_id() const noexcept {
    return m_defaultId;
}

bool MaterialRegistry::contains(MaterialId id) const noexcept {
    return is_valid(id);
}

std::size_t MaterialRegistry::size() const noexcept {
    return m_materials.size();
}

const std::vector<Material>& MaterialRegistry::materials() const noexcept {
    return m_materials;
}

bool MaterialRegistry::is_valid(MaterialId id) const noexcept {
    return id.value < m_materials.size();
}
} // namespace vulkano::scene
