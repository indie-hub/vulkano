#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vulkano/scene/mesh.hpp>

#include <glm/vec3.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/geometric.hpp>

namespace {
constexpr float EPS {1e-5F};
}

TEST_CASE("plane mesh has four vertices and unit normals") {
    const glm::vec3 color {0.5F, 0.5F, 0.5F};
    const auto mesh = vulkano::scene::MeshFactory::create_plane(2.0F, color);

    REQUIRE(mesh.vertices.size() == 4U);
    REQUIRE(mesh.indices.size() == 6U);

    for (const auto& vertex : mesh.vertices) {
        REQUIRE(glm::all(glm::epsilonEqual(vertex.normal, glm::vec3 {0.0F, 1.0F, 0.0F}, EPS)));
        REQUIRE(vertex.color == color);
    }
}

TEST_CASE("cube mesh builds 6 faces with outward normals") {
    const auto mesh = vulkano::scene::MeshFactory::create_cube(2.0F, glm::vec3 {1.0F, 0.0F, 0.0F});

    REQUIRE(mesh.vertices.size() == 24U);
    REQUIRE(mesh.indices.size() == 36U);

    for (std::size_t face {0U}; face < 6U; ++face) {
        const std::uint32_t base {static_cast<std::uint32_t>(face * 4U)};
        const glm::vec3 normal = mesh.vertices[base].normal;
        for (std::uint32_t i {0U}; i < 4U; ++i) {
            REQUIRE(glm::all(glm::epsilonEqual(mesh.vertices[base + i].normal, normal, EPS)));
        }
    }
}

TEST_CASE("sphere mesh respects segment counts") {
    const std::uint32_t longitude {16U};
    const std::uint32_t latitude {8U};
    const auto mesh = vulkano::scene::MeshFactory::create_uv_sphere(1.0F, longitude, latitude, glm::vec3 {0.0F, 0.0F, 1.0F});

    const std::size_t expectedVertices = static_cast<std::size_t>((latitude + 1U) * (longitude + 1U));
    const std::size_t expectedIndices = static_cast<std::size_t>(latitude * longitude * 6U);

    REQUIRE(mesh.vertices.size() == expectedVertices);
    REQUIRE(mesh.indices.size() == expectedIndices);

    for (const auto& vertex : mesh.vertices) {
        const float length = glm::length(vertex.normal);
        REQUIRE(length == Catch::Approx(1.0F).margin(EPS));
    }
}
