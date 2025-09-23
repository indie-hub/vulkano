#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <vector>

#include <glm/geometric.hpp>

#include <vulkano/primitives.hpp>

using Catch::Approx;

namespace {
    auto all_unit_normals(const vulkano::MeshData& mesh, float epsilon = 1e-4F) -> bool {
        for(const vulkano::MeshVertex& vertex : mesh.vertices) {
            const float length = glm::length(vertex.normal);
            if(!(length == Approx(1.0F).margin(epsilon))) {
                return false;
            }
        }
        return true;
    }

    auto all_valid_tangents(const vulkano::MeshData& mesh, float epsilon = 1e-4F) -> bool {
        for(const vulkano::MeshVertex& vertex : mesh.vertices) {
            const glm::vec3 tangent = glm::vec3 {vertex.tangent};
            if(!(glm::length(tangent) == Approx(1.0F).margin(epsilon))) {
                return false;
            }
            const float handedness = vertex.tangent.w;
            if(!(std::abs(std::abs(handedness) - 1.0F) <= epsilon)) {
                return false;
            }
            const glm::vec3 bitangent = glm::cross(vertex.normal, tangent) * handedness;
            if(!(glm::dot(bitangent, vertex.normal) == Approx(0.0F).margin(1e-3F))) {
                return false;
            }
        }
        return true;
    }

    auto expected_power_of_four(std::uint32_t exponent) -> std::size_t {
        std::size_t value {1U};
        for(std::uint32_t index {0U}; index < exponent; ++index) {
            value *= 4U;
        }
        return value;
    }

    auto expected_icosphere_vertices(std::uint32_t subdivisions) -> std::size_t {
        if(subdivisions == 0U) {
            return 12U;
        }
        // Derived from recursive subdivision pattern: V = 10 * 4^s + 2
        const std::size_t power = expected_power_of_four(subdivisions);
        return (10U * power) + 2U;
    }

    auto expected_icosphere_triangles(std::uint32_t subdivisions) -> std::size_t {
        // Triangles = 20 * 4^s
        const std::size_t power = expected_power_of_four(subdivisions);
        return 20U * power;
    }
}

TEST_CASE("Plane primitive generates expected quad", "[primitives]") {
    const vulkano::PlaneParameters parameters {
        .width = 2.0F,
        .depth = 4.0F,
        .uvTiling = glm::vec2 {2.0F, 1.0F}
    };
    const vulkano::PlanePrimitive plane {parameters};
    const vulkano::MeshData& mesh = plane.mesh();

    REQUIRE(mesh.vertices.size() == 4U);
    REQUIRE(mesh.indices.size() == 6U);
    REQUIRE(all_unit_normals(mesh));
    REQUIRE(all_valid_tangents(mesh));

    CHECK(mesh.vertices.front().position.x == Approx(-1.0F));
    CHECK(mesh.vertices.front().uv.x == Approx(0.0F));
    CHECK(mesh.vertices.at(2).uv.x == Approx(parameters.uvTiling.x));
}

TEST_CASE("Cube primitive exposes per-face vertices", "[primitives]") {
    const vulkano::CubePrimitive cube {};
    const vulkano::MeshData& mesh = cube.mesh();

    REQUIRE(mesh.vertices.size() == 24U);
    REQUIRE(mesh.indices.size() == 36U);
    REQUIRE(all_unit_normals(mesh));
    REQUIRE(all_valid_tangents(mesh));

    for(std::size_t face {0U}; face < mesh.vertices.size(); face += 4U) {
        const glm::vec3& normal = mesh.vertices.at(face).normal;
        for(std::size_t offset {1U}; offset < 4U; ++offset) {
            CHECK(glm::dot(normal, mesh.vertices.at(face + offset).normal) == Approx(1.0F).margin(1e-4));
        }
    }
}

TEST_CASE("Icosphere primitive scales with subdivisions", "[primitives]") {
    const vulkano::IcosphereParameters baseParameters {
        .subdivisions = 0U
    };
    const vulkano::IcospherePrimitive baseSphere {baseParameters};
    const vulkano::MeshData& baseMesh = baseSphere.mesh();

    REQUIRE(baseMesh.vertices.size() == expected_icosphere_vertices(baseParameters.subdivisions));
    REQUIRE(baseMesh.indices.size() == expected_icosphere_triangles(baseParameters.subdivisions) * 3U);
    REQUIRE(all_unit_normals(baseMesh));
    REQUIRE(all_valid_tangents(baseMesh));

    vulkano::IcospherePrimitive refinedSphere {{.subdivisions = 2U}};
    const vulkano::MeshData& refinedMesh = refinedSphere.mesh();
    REQUIRE(refinedMesh.vertices.size() == expected_icosphere_vertices(2U));
    REQUIRE(refinedMesh.indices.size() == expected_icosphere_triangles(2U) * 3U);
    REQUIRE(all_unit_normals(refinedMesh));
    REQUIRE(all_valid_tangents(refinedMesh));
}
