#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vulkano/app/scene_renderer.hpp>
#include <vulkano/scene/transform.hpp>

namespace {
vulkano::scene::Transform compose_transform(
    const vulkano::scene::Transform& parent, const vulkano::scene::Transform& local) {
    vulkano::scene::Transform composed {};
    composed.position = parent.position + local.position;
    composed.rotation = glm::normalize(parent.rotation * local.rotation);
    composed.scale = parent.scale * local.scale;
    return composed;
}

void gather_mesh_world_transforms(const vulkano::app::SceneRenderer::SceneNode& node,
    const vulkano::scene::Transform& parent, std::vector<glm::mat4>& out) {
    const vulkano::scene::Transform world = compose_transform(parent, node.transform);
    if (node.is_mesh()) {
        out.push_back(world.matrix());
    }
    for (const auto& child : node.children) {
        gather_mesh_world_transforms(child, world, out);
    }
}
}

TEST_CASE("SceneNode classification distinguishes groups and meshes") {
    vulkano::app::SceneRenderer::SceneNode group {};
    group.name = "Group";
    REQUIRE(group.is_group());
    REQUIRE_FALSE(group.is_mesh());

    vulkano::app::SceneRenderer::SceneNode meshNode {};
    meshNode.geometry.emplace();
    meshNode.geometry->material = vulkano::scene::MaterialId {1U};
    REQUIRE(meshNode.is_mesh());
    REQUIRE_FALSE(meshNode.is_group());
}

TEST_CASE("SceneNode hierarchy composes transforms for mesh children") {
    using Node = vulkano::app::SceneRenderer::SceneNode;

    Node root {};
    root.name = "Root";

    Node group {};
    group.name = "Group";
    group.transform.position = glm::vec3 {2.0F, 0.0F, 0.0F};

    Node meshNode {};
    meshNode.name = "Mesh";
    meshNode.transform.position = glm::vec3 {0.0F, 1.0F, 0.0F};
    meshNode.transform.scale = glm::vec3 {1.0F, 2.0F, 1.0F};
    meshNode.geometry.emplace();
    meshNode.geometry->material = vulkano::scene::MaterialId {2U};

    group.children.push_back(meshNode);
    root.children.push_back(group);

    std::vector<glm::mat4> worldTransforms;
    gather_mesh_world_transforms(root, vulkano::scene::Transform::identity(), worldTransforms);

    REQUIRE(worldTransforms.size() == 1U);
    const glm::mat4 world = worldTransforms.front();
    const vulkano::scene::Transform groupWorld = compose_transform(root.transform, group.transform);
    const vulkano::scene::Transform meshWorld = compose_transform(groupWorld, meshNode.transform);
    const glm::mat4 expected = meshWorld.matrix();
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            INFO("Mismatch at [" << column << "][" << row << "]");
            REQUIRE(world[column][row] == Catch::Approx(expected[column][row]).margin(1e-4F));
        }
    }
}

TEST_CASE("Parent rotation preserves child translation in world space") {
    vulkano::scene::Transform parent = vulkano::scene::Transform::identity();
    parent.set_euler_degrees(glm::vec3 {0.0F, 45.0F, 0.0F});

    vulkano::scene::Transform child = vulkano::scene::Transform::identity();
    child.position = glm::vec3 {3.0F, 0.0F, 0.0F};

    const vulkano::scene::Transform world = compose_transform(parent, child);
    REQUIRE(world.position.x == Catch::Approx(parent.position.x + child.position.x).margin(1e-5F));
    REQUIRE(world.position.y == Catch::Approx(parent.position.y + child.position.y).margin(1e-5F));
    REQUIRE(world.position.z == Catch::Approx(parent.position.z + child.position.z).margin(1e-5F));
    // Rotations still compose.
    const glm::mat4 expectedRotation = parent.matrix() * child.matrix();
    const glm::mat4 actualRotation = world.matrix();
    // Compare orientation by checking basis vectors after removing translation.
    for (int column = 0; column < 3; ++column) {
        for (int row = 0; row < 3; ++row) {
            INFO("Mismatch at basis [" << column << "][" << row << "]");
            REQUIRE(actualRotation[column][row] == Catch::Approx(expectedRotation[column][row]).margin(1e-4F));
        }
    }
}
