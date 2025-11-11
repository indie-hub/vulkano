#include <vulkano/app/asset_importer.hpp>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include <cstring>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace vulkano::app {
namespace {
[[nodiscard]] std::string make_embedded_prefix(std::string_view scenePath) {
    std::string canonical {"scene"};
    std::string label {"scene"};
    if (!scenePath.empty()) {
        const std::filesystem::path sourcePath {std::string {scenePath}};
        if (!sourcePath.empty()) {
            label = sourcePath.filename().empty() ? sourcePath.generic_string() : sourcePath.filename().generic_string();
            try {
                const std::filesystem::path absolute = std::filesystem::absolute(sourcePath).lexically_normal();
                canonical = absolute.generic_string();
            } catch (...) {
                canonical = sourcePath.generic_string();
            }
        }
    }

    const std::size_t hash = std::hash<std::string> {}(canonical);
    std::stringstream stream {};
    stream << "embedded://" << label << ':' << std::hex << hash;
    return stream.str();
}

[[nodiscard]] std::string remap_embedded_path(const std::string& prefix, std::string_view rawKey) {
    std::string remapped = prefix;
    remapped.push_back('#');
    remapped.append(rawKey);
    return remapped;
}

[[nodiscard]] glm::mat4 to_glm(const aiMatrix4x4& m) noexcept {
    return glm::mat4 {
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4
    };
}

[[nodiscard]] glm::vec3 to_vec3(const aiColor3D& color) noexcept {
    return glm::vec3 {color.r, color.g, color.b};
}

[[nodiscard]] glm::vec3 to_vec3(const aiColor4D& color) noexcept {
    return glm::vec3 {color.r, color.g, color.b};
}

[[nodiscard]] glm::vec3 to_vec3(const aiVector3D& vec) noexcept {
    return glm::vec3 {vec.x, vec.y, vec.z};
}

[[nodiscard]] std::string embedded_key(const aiTexture& texture, unsigned int index) {
    if (texture.mFilename.length > 0) {
        return texture.mFilename.C_Str();
    }
    return "*" + std::to_string(index);
}

[[nodiscard]] TextureData load_embedded_texture(const aiTexture& texture) {
    if (texture.mHeight == 0U) {
        const std::size_t byteCount = static_cast<std::size_t>(texture.mWidth);
        if (byteCount == 0U) {
            throw std::runtime_error {"Embedded texture is empty"};
        }

        std::vector<std::uint8_t> buffer(byteCount);
        std::memcpy(buffer.data(), texture.pcData, byteCount);
        return load_texture_from_memory(buffer.data(), buffer.size(), TextureColorSpace::sRGB, false,
            std::string_view {texture.achFormatHint, std::strlen(texture.achFormatHint)});
    }

    if (texture.mWidth == 0U) {
        throw std::runtime_error {"Embedded texture has zero dimensions"};
    }

    TextureData data {};
    data.width = texture.mWidth;
    data.height = texture.mHeight;
    data.channels = TextureChannels::RGBA;
    data.colorSpace = TextureColorSpace::sRGB;
    const std::size_t pixelCount = static_cast<std::size_t>(data.width) * static_cast<std::size_t>(data.height);
    data.pixels.resize(pixelCount * 4U);
    for (std::size_t idx {0U}; idx < pixelCount; ++idx) {
        const aiTexel& texel = texture.pcData[idx];
        data.pixels[idx * 4U + 0U] = texel.r;
        data.pixels[idx * 4U + 1U] = texel.g;
        data.pixels[idx * 4U + 2U] = texel.b;
        data.pixels[idx * 4U + 3U] = texel.a;
    }
    return data;
}
} // namespace

std::string AssetImporter::resolve_texture_path(std::string_view rawPath, const std::filesystem::path& baseDirectory) {
    if (rawPath.empty()) {
        return std::string {rawPath};
    }
    if (!rawPath.empty() && rawPath.front() == '*') {
        return std::string {rawPath};
    }

    std::filesystem::path texturePath {std::string {rawPath}};
    if (texturePath.is_absolute()) {
        try {
            return std::filesystem::weakly_canonical(texturePath).string();
        } catch (...) {
            return texturePath.lexically_normal().string();
        }
    }

    if (!baseDirectory.empty()) {
        std::filesystem::path combined = baseDirectory / texturePath;
        try {
            combined = std::filesystem::weakly_canonical(combined);
        } catch (...) {
            combined = combined.lexically_normal();
        }
        return combined.string();
    }

    return texturePath.lexically_normal().string();
}

ImportedScene AssetImporter::load_scene(std::string_view path) const {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(std::string {path},
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace
            | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices | aiProcess_Debone);

    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || scene->mRootNode == nullptr) {
        throw std::runtime_error {std::string {"Failed to load scene: "} + importer.GetErrorString()};
    }

    ImportedScene result {};
    std::filesystem::path sourceDirectory {};
    try {
        const std::filesystem::path sourcePath {std::string {path}};
        if (sourcePath.has_parent_path()) {
            sourceDirectory = std::filesystem::weakly_canonical(sourcePath.parent_path());
        } else {
            sourceDirectory = std::filesystem::weakly_canonical(std::filesystem::current_path());
        }
    } catch (...) {
        sourceDirectory.clear();
    }
    result.sourceDirectory = sourceDirectory;

    result.root.name = scene->mRootNode->mName.length > 0 ? scene->mRootNode->mName.C_Str() : "Imported Scene";
    result.root.transform = scene::Transform::from_matrix(to_glm(scene->mRootNode->mTransformation));

    const std::string embeddedPrefix = make_embedded_prefix(path);
    std::unordered_map<std::string, std::string> embeddedKeyRemap {};
    embeddedKeyRemap.reserve(scene->mNumTextures);

    result.embeddedTextures.clear();
    result.embeddedTextures.reserve(scene->mNumTextures);
    for (unsigned int textureIndex {0U}; textureIndex < scene->mNumTextures; ++textureIndex) {
        const aiTexture* texture = scene->mTextures[textureIndex];
        if (texture == nullptr) {
            continue;
        }
        const std::string rawKey = embedded_key(*texture, textureIndex);
        const std::string remappedKey = remap_embedded_path(embeddedPrefix, rawKey);
        embeddedKeyRemap.emplace(rawKey, remappedKey);
        try {
            result.embeddedTextures.emplace(remappedKey, load_embedded_texture(*texture));
        } catch (const std::exception&) {
            result.embeddedTextures.emplace(remappedKey,
                make_solid_texture(glm::vec4 {1.0F, 1.0F, 1.0F, 1.0F}, TextureColorSpace::sRGB));
        }
    }

    result.materials.reserve(scene->mNumMaterials);
    for (unsigned int index {0U}; index < scene->mNumMaterials; ++index) {
        result.materials.push_back(import_material(*scene->mMaterials[index], result.sourceDirectory));
    }

    const auto remap_material_paths = [&embeddedKeyRemap](scene::Material& material) {
        if (embeddedKeyRemap.empty()) {
            return;
        }
        const auto update = [&embeddedKeyRemap](std::string& value) {
            if (value.empty()) {
                return;
            }
            const auto it = embeddedKeyRemap.find(value);
            if (it != embeddedKeyRemap.end()) {
                value = it->second;
            }
        };

        update(material.textures.baseColorPath);
        update(material.textures.normalPath);
        update(material.textures.metallicRoughnessPath);
        update(material.textures.ambientOcclusionPath);
        update(material.textures.surfacePropertiesPath);
    };

    for (ImportedMaterial& imported : result.materials) {
        remap_material_paths(imported.material);
    }

    build_node(*scene, *scene->mRootNode, glm::mat4(1.0F), result.root);

    const bool hasMeshes = !result.root.meshes.empty();
    const bool hasChildren = !result.root.children.empty();
    if (!hasMeshes && !hasChildren) {
        throw std::runtime_error {"Imported scene contains no meshes"};
    }

    return result;
}

ImportedMaterial AssetImporter::import_material(const aiMaterial& material, const std::filesystem::path& baseDirectory) {
    ImportedMaterial result {};
    scene::Material& output = result.material;

    aiString name;
    if (material.Get(AI_MATKEY_NAME, name) == aiReturn_SUCCESS) {
        (void)name;
    }

    aiColor3D color {1.0F, 1.0F, 1.0F};
    if (material.Get(AI_MATKEY_COLOR_DIFFUSE, color) == aiReturn_SUCCESS) {
        output.properties.baseColor = to_vec3(color);
    }

    float metallic {0.0F};
    if (material.Get(AI_MATKEY_METALLIC_FACTOR, metallic) == aiReturn_SUCCESS) {
        output.properties.metallic = metallic;
    }

    float roughness {0.5F};
    if (material.Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == aiReturn_SUCCESS) {
        output.properties.roughness = roughness;
    }

    aiColor3D emissive {0.0F, 0.0F, 0.0F};
    if (material.Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == aiReturn_SUCCESS) {
        output.properties.emissive = to_vec3(emissive);
    }

    float emissiveIntensity {0.0F};
    if (material.Get(AI_MATKEY_EMISSIVE_INTENSITY, emissiveIntensity) == aiReturn_SUCCESS) {
        output.properties.emissiveIntensity = emissiveIntensity;
    }

    auto extract_path = [&material, &baseDirectory](aiTextureType type, std::string& destination,
                              bool& usageFlag) {
        aiString texturePath;
        if (material.GetTextureCount(type) > 0 && material.GetTexture(type, 0, &texturePath) == aiReturn_SUCCESS) {
            const std::string raw = texturePath.C_Str();
            if (!raw.empty() && raw.front() == '*') {
                destination = raw;
            } else {
                destination = AssetImporter::resolve_texture_path(raw, baseDirectory);
            }
            usageFlag = true;
        }
    };

    extract_path(aiTextureType_BASE_COLOR, output.textures.baseColorPath, output.useBaseColorTexture);
    extract_path(aiTextureType_DIFFUSE, output.textures.baseColorPath, output.useBaseColorTexture);
    extract_path(aiTextureType_NORMALS, output.textures.normalPath, output.useNormalTexture);
    extract_path(aiTextureType_HEIGHT, output.textures.normalPath, output.useNormalTexture);
    extract_path(aiTextureType_METALNESS, output.textures.metallicRoughnessPath, output.useMetallicRoughnessTexture);
    extract_path(aiTextureType_DIFFUSE_ROUGHNESS, output.textures.metallicRoughnessPath, output.useMetallicRoughnessTexture);
    extract_path(aiTextureType_AMBIENT_OCCLUSION, output.textures.ambientOcclusionPath, output.useAmbientOcclusionTexture);

    return result;
}

ImportedMesh AssetImporter::import_mesh(const aiMesh& mesh, std::uint32_t materialIndex) {
    if (!mesh.HasPositions()) {
        throw std::runtime_error {"Mesh is missing vertex positions"};
    }
    if (!mesh.HasFaces()) {
        throw std::runtime_error {"Mesh has no faces"};
    }

    ImportedMesh result {};
    result.materialIndex = materialIndex;
    if (mesh.mName.length > 0U) {
        result.name = mesh.mName.C_Str();
    }

    scene::MeshData meshData {};
    meshData.vertices.resize(mesh.mNumVertices);

    for (unsigned int v {0U}; v < mesh.mNumVertices; ++v) {
        scene::Vertex& vertex = meshData.vertices[v];
        vertex.position = to_vec3(mesh.mVertices[v]);
        vertex.normal = mesh.HasNormals() ? to_vec3(mesh.mNormals[v]) : glm::vec3 {0.0F, 1.0F, 0.0F};
        vertex.color = mesh.HasVertexColors(0) ? to_vec3(mesh.mColors[0][v]) : glm::vec3 {1.0F, 1.0F, 1.0F};
        vertex.uv = mesh.HasTextureCoords(0) ? glm::vec2 {mesh.mTextureCoords[0][v].x, mesh.mTextureCoords[0][v].y}
                                             : glm::vec2 {0.0F, 0.0F};
        vertex.tangent = mesh.HasTangentsAndBitangents() ? to_vec3(mesh.mTangents[v]) : glm::vec3 {1.0F, 0.0F, 0.0F};
        vertex.bitangentSign = 1.0F;
    }

    meshData.indices.reserve(mesh.mNumFaces * 3U);
    for (unsigned int f {0U}; f < mesh.mNumFaces; ++f) {
        const aiFace& face = mesh.mFaces[f];
        if (face.mNumIndices != 3U) {
            throw std::runtime_error {"Non-triangulated face encountered"};
        }
        for (unsigned int i {0U}; i < face.mNumIndices; ++i) {
            meshData.indices.push_back(face.mIndices[i]);
        }
    }

    scene::generate_tangent_frames(meshData);

    result.mesh = std::move(meshData);
    return result;
}

void AssetImporter::build_node(const aiScene& scene, const aiNode& node, const glm::mat4& parentWorld,
    ImportedScene::Node& output) {
    output.name = node.mName.length > 0 ? node.mName.C_Str() : "Node";
    const glm::mat4 local = to_glm(node.mTransformation);
    output.transform = scene::Transform::from_matrix(local);

    output.meshes.clear();
    output.meshes.reserve(node.mNumMeshes);
    for (unsigned int i {0U}; i < node.mNumMeshes; ++i) {
        const unsigned int meshIndex = node.mMeshes[i];
        const aiMesh* mesh = scene.mMeshes[meshIndex];
        const std::uint32_t materialIndex = mesh->mMaterialIndex < scene.mNumMaterials ? mesh->mMaterialIndex : 0U;
        output.meshes.push_back(import_mesh(*mesh, materialIndex));
    }

    output.children.clear();
    output.children.reserve(node.mNumChildren);
    const glm::mat4 world = parentWorld * local;
    for (unsigned int i {0U}; i < node.mNumChildren; ++i) {
        ImportedScene::Node child {};
        build_node(scene, *node.mChildren[i], world, child);
        output.children.push_back(std::move(child));
    }
}
} // namespace vulkano::app
