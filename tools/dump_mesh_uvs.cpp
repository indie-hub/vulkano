#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <vulkano/app/asset_importer.hpp>
#include <vulkano/scene/mesh.hpp>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

namespace {
struct UvStats final {
    std::string meshLabel {};
    std::size_t vertexCount {0U};
    bool hasMaterial {false};
    std::uint32_t materialIndex {0U};
    bool hasUvs {false};
    float minU {std::numeric_limits<float>::infinity()};
    float maxU {-std::numeric_limits<float>::infinity()};
    float minV {std::numeric_limits<float>::infinity()};
    float maxV {-std::numeric_limits<float>::infinity()};
};

void accumulate_stats(const vulkano::scene::MeshData& mesh, UvStats& stats) noexcept {
    stats.vertexCount = mesh.vertices.size();
    for (const vulkano::scene::Vertex& vertex : mesh.vertices) {
        stats.hasUvs = true;
        stats.minU = std::min(stats.minU, vertex.uv.x);
        stats.maxU = std::max(stats.maxU, vertex.uv.x);
        stats.minV = std::min(stats.minV, vertex.uv.y);
        stats.maxV = std::max(stats.maxV, vertex.uv.y);
    }
}

void traverse_node(const vulkano::app::ImportedScene::Node& node, std::string_view pathPrefix,
    const std::vector<vulkano::app::ImportedMaterial>& materials, std::vector<UvStats>& stats) {
    const std::string currentPath = pathPrefix.empty() ? node.name : std::string {pathPrefix} + "/" + node.name;
    for (std::size_t index {0U}; index < node.meshes.size(); ++index) {
        UvStats entry {};
        entry.meshLabel = currentPath + "#mesh" + std::to_string(index);
        entry.hasMaterial = node.meshes[index].materialIndex < materials.size();
        if (entry.hasMaterial) {
            entry.materialIndex = node.meshes[index].materialIndex;
        }
        accumulate_stats(node.meshes[index].mesh, entry);
        stats.push_back(std::move(entry));
    }
    for (const auto& child : node.children) {
        traverse_node(child, currentPath, materials, stats);
    }
}

[[nodiscard]] std::vector<UvStats> collect_stats(const vulkano::app::ImportedScene& scene) {
    std::vector<UvStats> stats {};
    traverse_node(scene.root, "", scene.materials, stats);
    return stats;
}

void print_stats(const std::vector<UvStats>& stats) {
    std::cout << "Mesh Count: " << stats.size() << '\n';
    for (const UvStats& stat : stats) {
        std::cout << "- " << stat.meshLabel << '\n';
        std::cout << "  vertices: " << stat.vertexCount << '\n';
        if (stat.hasMaterial) {
            std::cout << "  material index: " << stat.materialIndex << '\n';
        } else {
            std::cout << "  material index: <invalid>\n";
        }
        if (!stat.hasUvs) {
            std::cout << "  uvs: <missing>\n";
            continue;
        }
        std::cout << std::fixed << std::setprecision(6);
        std::cout << "  u range: [" << stat.minU << ", " << stat.maxU << "]\n";
        std::cout << "  v range: [" << stat.minV << ", " << stat.maxV << "]\n";
    }
}

struct AssimpStats final {
    std::string meshLabel {};
    std::size_t vertexCount {0U};
    std::uint32_t materialIndex {0U};
    bool hasUvs {false};
    float minU {std::numeric_limits<float>::infinity()};
    float maxU {-std::numeric_limits<float>::infinity()};
    float minV {std::numeric_limits<float>::infinity()};
    float maxV {-std::numeric_limits<float>::infinity()};
};

[[nodiscard]] std::vector<AssimpStats> collect_assimp_stats(const aiScene& scene) {
    std::vector<AssimpStats> stats {};
    stats.reserve(scene.mNumMeshes);
    for (unsigned int i {0U}; i < scene.mNumMeshes; ++i) {
        const aiMesh* mesh = scene.mMeshes[i];
        AssimpStats entry {};
        entry.meshLabel = "mesh[" + std::to_string(i) + "]";
        entry.vertexCount = mesh->mNumVertices;
        entry.materialIndex = mesh->mMaterialIndex;
        if (!mesh->HasTextureCoords(0)) {
            stats.push_back(std::move(entry));
            continue;
        }
        entry.hasUvs = true;
        for (unsigned int v {0U}; v < mesh->mNumVertices; ++v) {
            const aiVector3D& uv = mesh->mTextureCoords[0][v];
            entry.minU = std::min(entry.minU, uv.x);
            entry.maxU = std::max(entry.maxU, uv.x);
            entry.minV = std::min(entry.minV, uv.y);
            entry.maxV = std::max(entry.maxV, uv.y);
        }
        stats.push_back(std::move(entry));
    }
    return stats;
}

void print_assimp_stats(std::string_view label, const std::vector<AssimpStats>& stats) {
    std::cout << label << '\n';
    std::cout << "  Mesh Count: " << stats.size() << '\n';
    for (const AssimpStats& stat : stats) {
        std::cout << "  - " << stat.meshLabel << '\n';
        std::cout << "    vertices: " << stat.vertexCount << '\n';
        std::cout << "    material index: " << stat.materialIndex << '\n';
        if (!stat.hasUvs) {
            std::cout << "    uvs: <missing>\n";
            continue;
        }
        std::cout << std::fixed << std::setprecision(6);
        std::cout << "    u range: [" << stat.minU << ", " << stat.maxU << "]\n";
        std::cout << "    v range: [" << stat.minV << ", " << stat.maxV << "]\n";
    }
}

void compare_flip_settings(const std::filesystem::path& assetPath) {
    Assimp::Importer importer {};
    unsigned int baseFlags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace
        | aiProcess_JoinIdenticalVertices | aiProcess_Debone;

    const aiScene* unflippedScene = importer.ReadFile(assetPath.string(), baseFlags);
    if (unflippedScene == nullptr) {
        throw std::runtime_error {std::string {"Assimp failed without UV flip: "} + importer.GetErrorString()};
    }
    std::vector<AssimpStats> unflipped = collect_assimp_stats(*unflippedScene);

    importer.FreeScene();
    const aiScene* flippedScene = importer.ReadFile(assetPath.string(), baseFlags | aiProcess_FlipUVs);
    if (flippedScene == nullptr) {
        throw std::runtime_error {std::string {"Assimp failed with UV flip: "} + importer.GetErrorString()};
    }
    std::vector<AssimpStats> flipped = collect_assimp_stats(*flippedScene);

    print_assimp_stats("Assimp (no aiProcess_FlipUVs)", unflipped);
    print_assimp_stats("Assimp (with aiProcess_FlipUVs)", flipped);
}
} // namespace

int main(int argc, char** argv) {
    bool compareFlip {false};
    std::filesystem::path assetPath {};
    for (int argIndex {1}; argIndex < argc; ++argIndex) {
        const std::string_view argument {argv[argIndex]};
        if (argument == "--compare-uv-flip") {
            compareFlip = true;
            continue;
        }
        if (!assetPath.empty()) {
            std::cerr << "Unexpected argument: " << argument << '\n';
            std::cerr << "Usage: dump_mesh_uvs [--compare-uv-flip] <path-to-asset>\n";
            return EXIT_FAILURE;
        }
        assetPath = argument;
    }

    if (assetPath.empty()) {
        std::cerr << "Usage: dump_mesh_uvs [--compare-uv-flip] <path-to-asset>\n";
        return EXIT_FAILURE;
    }

    if (!std::filesystem::exists(assetPath)) {
        std::cerr << "Asset file does not exist: " << assetPath << '\n';
        return EXIT_FAILURE;
    }

    if (compareFlip) {
        try {
            compare_flip_settings(assetPath);
        } catch (const std::exception& ex) {
            std::cerr << "Error: " << ex.what() << '\n';
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    vulkano::app::AssetImporter importer {};
    vulkano::app::ImportedScene scene = importer.load_scene(assetPath.string());

    const std::vector<UvStats> stats = collect_stats(scene);
    print_stats(stats);
    return EXIT_SUCCESS;
}
