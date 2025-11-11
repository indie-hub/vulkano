#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <filesystem>

#include <vulkano/app/asset_importer.hpp>
#include <vulkano/app/mesh_packing.hpp>

#include <glm/vec2.hpp>

namespace {
[[nodiscard]] const vulkano::scene::MeshData& first_mesh(const vulkano::app::ImportedScene::Node& node) {
    if (!node.meshes.empty()) {
        return node.meshes.front().mesh;
    }
    for (const auto& child : node.children) {
        const vulkano::scene::MeshData& candidate = first_mesh(child);
        if (!candidate.vertices.empty()) {
            return candidate;
        }
    }
    static const vulkano::scene::MeshData empty {};
    return empty;
}
} // namespace

TEST_CASE("Mesh packer preserves importer UV coordinates") {
    const vulkano::app::AssetImporter importer {};
    const auto scene = importer.load_scene(TEST_ASSET_DIR "/uv_triangle.obj");
    const vulkano::scene::MeshData& mesh = first_mesh(scene.root);
    REQUIRE(mesh.vertices.size() == 3U);

    const auto packed = vulkano::app::pack_mesh_vertices(mesh);
    REQUIRE(packed.size() == mesh.vertices.size());

    using Catch::Matchers::WithinAbs;
    constexpr float epsilon {1e-5F};

    const std::array<glm::vec2, 3> expected {
        glm::vec2 {0.0F, 1.0F},
        glm::vec2 {1.0F, 1.0F},
        glm::vec2 {0.0F, 0.0F}
    };

    for (std::size_t index {0U}; index < mesh.vertices.size(); ++index) {
        const glm::vec2& sourceUv = mesh.vertices[index].uv;
        const glm::vec2& packedUv = packed[index].uv;
        INFO("sourceUv[" << index << "] = (" << sourceUv.x << ", " << sourceUv.y << ")");
        INFO("packedUv[" << index << "] = (" << packedUv.x << ", " << packedUv.y << ")");
        REQUIRE_THAT(sourceUv.x, WithinAbs(expected[index].x, epsilon));
        REQUIRE_THAT(sourceUv.y, WithinAbs(expected[index].y, epsilon));
        REQUIRE_THAT(packedUv.x, WithinAbs(sourceUv.x, epsilon));
        REQUIRE_THAT(packedUv.y, WithinAbs(sourceUv.y, epsilon));
    }
}
