#pragma once

#include <string_view>
#include <unordered_map>
#include <vector>

#include <assimp/scene.h>

#include <glm/mat4x4.hpp>

#include <vulkano/scene/material.hpp>
#include <vulkano/scene/mesh.hpp>
#include <vulkano/scene/transform.hpp>
#include <vulkano/app/texture_loader.hpp>

namespace vulkano::app {
struct ImportedMaterial final {
    scene::Material material {};
};

struct ImportedMesh final {
    scene::MeshData mesh {};
    std::uint32_t materialIndex {0U};
    scene::Transform transform {};
};

struct ImportedScene final {
    std::vector<ImportedMaterial> materials;
    std::vector<ImportedMesh> meshes;
    std::unordered_map<std::string, TextureData> embeddedTextures;
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
