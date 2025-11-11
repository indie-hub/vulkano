#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkano/app/scene_renderer.hpp>

namespace {
[[nodiscard]] glm::mat4 compose_world(const vulkano::app::SceneRenderer::SceneNode& node,
    const glm::mat4& parent) {
    return parent * node.transform.matrix();
}

void gather_mesh_world_transforms(const vulkano::app::SceneRenderer::SceneNode& node, const glm::mat4& parent,
    std::vector<glm::mat4>& out) {
    const glm::mat4 world = compose_world(node, parent);
    if (node.is_mesh()) {
        out.push_back(world);
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
    gather_mesh_world_transforms(root, glm::mat4 {1.0F}, worldTransforms);

    REQUIRE(worldTransforms.size() == 1U);
    const glm::mat4 world = worldTransforms.front();
    const glm::mat4 expected = root.transform.matrix() * group.transform.matrix() * meshNode.transform.matrix();
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            INFO("Mismatch at [" << column << "][" << row << "]");
            REQUIRE(world[column][row] == Catch::Approx(expected[column][row]).margin(1e-4F));
        }
    }
}
