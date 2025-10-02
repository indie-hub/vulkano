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
