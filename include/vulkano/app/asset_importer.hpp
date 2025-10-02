#pragma once

#include <string_view>
#include <vector>

#include <assimp/scene.h>

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

private:
    [[nodiscard]] static ImportedMaterial import_material(const aiMaterial& material);
    [[nodiscard]] static ImportedMesh import_mesh(const aiMesh& mesh, std::uint32_t materialIndex,
        const glm::mat4& transform);
    static void traverse_node(const aiScene& scene, const aiNode& node, const glm::mat4& parentTransform,
        std::vector<ImportedMesh>& meshes);
};
} // namespace vulkano::app
