#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <vulkano/app/asset_importer.hpp>
#include <vulkano/scene/mesh.hpp>

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
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: dump_mesh_uvs <path-to-asset>\n";
        return EXIT_FAILURE;
    }

    const std::filesystem::path assetPath {argv[1]};
    if (!std::filesystem::exists(assetPath)) {
        std::cerr << "Asset file does not exist: " << assetPath << '\n';
        return EXIT_FAILURE;
    }

    vulkano::app::AssetImporter importer {};
    vulkano::app::ImportedScene scene = importer.load_scene(assetPath.string());

    const std::vector<UvStats> stats = collect_stats(scene);
    print_stats(stats);
    return EXIT_SUCCESS;
}
