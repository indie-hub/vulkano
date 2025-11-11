#include <catch2/catch_test_macros.hpp>

#include <vulkano/app/asset_importer.hpp>

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
    REQUIRE(material.useBaseColorTexture);
    REQUIRE_FALSE(material.textures.baseColorPath.empty());
    REQUIRE(scene.embeddedTextures.count(material.textures.baseColorPath) == 1U);
    const auto& texture = scene.embeddedTextures.at(material.textures.baseColorPath);
    REQUIRE(texture.width == 1U);
    REQUIRE(texture.height == 1U);
    REQUIRE(texture.channels == vulkano::app::TextureChannels::RGBA);
    REQUIRE(texture.pixels.size() == 4U);
    CHECK(static_cast<int>(texture.pixels[0]) == 255);
    CHECK(static_cast<int>(texture.pixels[1]) == 255);
    CHECK(static_cast<int>(texture.pixels[2]) == 255);
    CHECK(static_cast<int>(texture.pixels[3]) == 255);
}
