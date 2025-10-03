#include <vulkano/app/asset_importer.hpp>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include <cstring>
#include <stdexcept>
#include <string_view>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace vulkano::app {
namespace {
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

ImportedScene AssetImporter::load_scene(std::string_view path) const {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(std::string {path},
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace
            | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices | aiProcess_Debone);

    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || scene->mRootNode == nullptr) {
        throw std::runtime_error {std::string {"Failed to load scene: "} + importer.GetErrorString()};
    }

    ImportedScene result {};
    result.name = scene->mRootNode->mName.length > 0 ? scene->mRootNode->mName.C_Str() : "Imported Scene";
    result.rootTransform = scene::Transform::identity();

    result.embeddedTextures.clear();
    result.embeddedTextures.reserve(scene->mNumTextures);
    for (unsigned int textureIndex {0U}; textureIndex < scene->mNumTextures; ++textureIndex) {
        const aiTexture* texture = scene->mTextures[textureIndex];
        if (texture == nullptr) {
            continue;
        }
        const std::string key = embedded_key(*texture, textureIndex);
        try {
            result.embeddedTextures.emplace(key, load_embedded_texture(*texture));
        } catch (const std::exception& ex) {
            result.embeddedTextures.emplace(key, make_solid_texture(glm::vec4 {1.0F, 1.0F, 1.0F, 1.0F}, TextureColorSpace::sRGB));
        }
    }

    result.materials.reserve(scene->mNumMaterials);
    for (unsigned int index {0U}; index < scene->mNumMaterials; ++index) {
        result.materials.push_back(import_material(*scene->mMaterials[index]));
    }

    result.meshes.reserve(scene->mNumMeshes);
    traverse_node(*scene, *scene->mRootNode, glm::mat4(1.0F), result.meshes);

    if (result.meshes.empty()) {
        throw std::runtime_error {"Imported scene contains no meshes"};
    }

    return result;
}

ImportedMaterial AssetImporter::import_material(const aiMaterial& material) {
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

    auto extract_path = [&material](aiTextureType type, std::string& destination, bool& usageFlag) {
        aiString texturePath;
        if (material.GetTextureCount(type) > 0 && material.GetTexture(type, 0, &texturePath) == aiReturn_SUCCESS) {
            const std::string raw = texturePath.C_Str();
            if (!raw.empty() && raw.front() == '*') {
                destination = raw;
            } else {
                destination = raw;
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

ImportedMesh AssetImporter::import_mesh(const aiMesh& mesh, std::uint32_t materialIndex, const glm::mat4& transform) {
    if (!mesh.HasPositions()) {
        throw std::runtime_error {"Mesh is missing vertex positions"};
    }
    if (!mesh.HasFaces()) {
        throw std::runtime_error {"Mesh has no faces"};
    }

    ImportedMesh result {};
    result.materialIndex = materialIndex;
    result.transform = scene::Transform::from_matrix(transform);
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

    result.mesh = std::move(meshData);
    return result;
}

void AssetImporter::traverse_node(const aiScene& scene, const aiNode& node, const glm::mat4& parentTransform,
    std::vector<ImportedMesh>& meshes) {
    const glm::mat4 nodeTransform = parentTransform * to_glm(node.mTransformation);

    for (unsigned int i {0U}; i < node.mNumMeshes; ++i) {
        const unsigned int meshIndex = node.mMeshes[i];
        const aiMesh* mesh = scene.mMeshes[meshIndex];
        const std::uint32_t materialIndex = mesh->mMaterialIndex < scene.mNumMaterials ? mesh->mMaterialIndex : 0U;
        meshes.push_back(import_mesh(*mesh, materialIndex, nodeTransform));
    }

    for (unsigned int i {0U}; i < node.mNumChildren; ++i) {
        traverse_node(scene, *node.mChildren[i], nodeTransform, meshes);
    }
}
} // namespace vulkano::app
