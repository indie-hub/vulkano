#pragma once

#include <string_view>
#include <vector>

#include <glm/mat4x4.hpp>

#include <vulkano/scene/material.hpp>
#include <vulkano/scene/mesh.hpp>

namespace vulkano::app {
struct ImportedMaterial final {
    scene::Material material {};
};

struct ImportedMesh final {
    scene::MeshData mesh {};
    std::uint32_t materialIndex {0U};
    glm::mat4 transform {1.0F};
};

struct ImportedScene final {
    std::vector<ImportedMaterial> materials;
    std::vector<ImportedMesh> meshes;
};

class AssetImporter final {
public:
    [[nodiscard]] ImportedScene load_scene(std::string_view path) const;
};
} // namespace vulkano::app
