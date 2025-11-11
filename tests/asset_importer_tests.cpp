#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <filesystem>

#include <vulkano/app/asset_importer.hpp>

#include <glm/geometric.hpp>

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

TEST_CASE("ImportedScene defaults to empty collections") {
    const vulkano::app::ImportedScene scene {};
    REQUIRE(scene.materials.empty());
    REQUIRE(scene.root.meshes.empty());
    REQUIRE(scene.root.children.empty());
}

TEST_CASE("AssetImporter throws until implemented") {
    const vulkano::app::AssetImporter importer {};
    CHECK_THROWS_AS(importer.load_scene("dummy.obj"), std::runtime_error);
}


TEST_CASE("AssetImporter loads simple OBJ") {
    const vulkano::app::AssetImporter importer {};
    const auto scene = importer.load_scene(TEST_ASSET_DIR "/hello_triangle.obj");
    bool foundMesh = false;
    const auto traverse = [&](const auto& self, const vulkano::app::ImportedScene::Node& node) -> void {
        for (const auto& mesh : node.meshes) {
            REQUIRE(mesh.mesh.vertices.size() == 3U);
            REQUIRE(mesh.mesh.indices.size() == 3U);
            foundMesh = true;
        }
        for (const auto& child : node.children) {
            self(self, child);
        }
    };
    traverse(traverse, scene.root);
    REQUIRE(foundMesh);
}


TEST_CASE("AssetImporter loads embedded texture") {
    const vulkano::app::AssetImporter importer {};
    const auto scene = importer.load_scene(TEST_ASSET_DIR "/embedded_triangle.gltf");
    REQUIRE(scene.materials.size() >= 1U);
    REQUIRE_FALSE(scene.embeddedTextures.empty());
    const auto& material = scene.materials.front().material;
    INFO("baseColorPath=" << material.textures.baseColorPath);
    REQUIRE(material.useBaseColorTexture);
    REQUIRE_FALSE(material.textures.baseColorPath.empty());
    const std::string& baseColorKey = material.textures.baseColorPath;
    REQUIRE(scene.embeddedTextures.count(baseColorKey) == 1U);
    CHECK(baseColorKey != "*0");
    CHECK(baseColorKey.find("*0") != std::string::npos);
    CHECK(baseColorKey.rfind("embedded://", 0) == 0U);
    const auto& texture = scene.embeddedTextures.at(baseColorKey);
    REQUIRE(texture.width == 1U);
    REQUIRE(texture.height == 1U);
    REQUIRE(texture.channels == vulkano::app::TextureChannels::RGBA);
    REQUIRE(texture.pixels.size() == 4U);
    CHECK(static_cast<int>(texture.pixels[0]) == 255);
    CHECK(static_cast<int>(texture.pixels[1]) == 255);
    CHECK(static_cast<int>(texture.pixels[2]) == 255);
    CHECK(static_cast<int>(texture.pixels[3]) == 255);
}

TEST_CASE("AssetImporter records scene source directory") {
    const vulkano::app::AssetImporter importer {};
    const auto scene = importer.load_scene(TEST_ASSET_DIR "/embedded_triangle.gltf");
    const std::filesystem::path expected = std::filesystem::weakly_canonical(std::filesystem::path {TEST_ASSET_DIR});
    REQUIRE(scene.sourceDirectory == expected);
}

TEST_CASE("AssetImporter resolves relative texture path against base directory") {
    const std::filesystem::path base = std::filesystem::weakly_canonical(std::filesystem::path {TEST_ASSET_DIR});
    const std::string resolved = vulkano::app::AssetImporter::resolve_texture_path("textures/albedo.png", base);
    const std::filesystem::path expected = base / "textures/albedo.png";
    REQUIRE(std::filesystem::path {resolved} == expected);
}

TEST_CASE("AssetImporter preserves embedded and absolute texture identifiers") {
    const std::filesystem::path base = std::filesystem::weakly_canonical(std::filesystem::path {TEST_ASSET_DIR});
    REQUIRE(vulkano::app::AssetImporter::resolve_texture_path("*0", base) == "*0");
    const std::filesystem::path absolute = std::filesystem::weakly_canonical(std::filesystem::path {TEST_ASSET_DIR}
        / "../../assets/textures/cube.png");
    REQUIRE(std::filesystem::path {vulkano::app::AssetImporter::resolve_texture_path(absolute.string(), base)}
        == absolute);
}

TEST_CASE("AssetImporter regenerates tangent frames when missing from source mesh") {
    const vulkano::app::AssetImporter importer {};
    const auto scene = importer.load_scene(TEST_ASSET_DIR "/uv_triangle.obj");
    const vulkano::scene::MeshData& mesh = first_mesh(scene.root);
    REQUIRE(mesh.vertices.size() == 3U);

    using Catch::Matchers::WithinAbs;
    constexpr float epsilon {1e-4F};

    for (const vulkano::scene::Vertex& vertex : mesh.vertices) {
        const float tangentLength = glm::length(vertex.tangent);
        REQUIRE_THAT(tangentLength, WithinAbs(1.0F, epsilon));
        REQUIRE_THAT(std::abs(vertex.bitangentSign), WithinAbs(1.0F, epsilon));
    }
}
