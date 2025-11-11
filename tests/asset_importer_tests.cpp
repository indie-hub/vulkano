#include <catch2/catch_test_macros.hpp>

#include <vulkano/app/asset_importer.hpp>

TEST_CASE("ImportedScene defaults to empty collections") {
    const vulkano::app::ImportedScene scene {};
    REQUIRE(scene.materials.empty());
    REQUIRE(scene.meshes.empty());
}

TEST_CASE("AssetImporter throws until implemented") {
    const vulkano::app::AssetImporter importer {};
    CHECK_THROWS_AS(importer.load_scene("dummy.obj"), std::runtime_error);
}


TEST_CASE("AssetImporter loads simple OBJ") {
    const vulkano::app::AssetImporter importer {};
    const auto scene = importer.load_scene(TEST_ASSET_DIR "/hello_triangle.obj");
    REQUIRE(scene.meshes.size() == 1U);
    REQUIRE(scene.materials.size() >= 1U);
    const auto& mesh = scene.meshes.front();
    REQUIRE(mesh.mesh.vertices.size() == 3U);
    REQUIRE(mesh.mesh.indices.size() == 3U);
}
