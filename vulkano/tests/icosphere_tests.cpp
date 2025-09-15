#include <catch2/catch_all.hpp>
#include <vulkano/icosphere.hpp>

using namespace vulkano;

TEST_CASE("Icosphere vertex/index counts increase with subdivision")
{
    const auto m0 = make_icosphere(0);
    const auto m1 = make_icosphere(1);
    const auto m2 = make_icosphere(2);
    REQUIRE(m0.vertex_count() > 0);
    REQUIRE(m0.index_count() % 3 == 0);
    REQUIRE(m1.vertex_count() > m0.vertex_count());
    REQUIRE(m2.vertex_count() > m1.vertex_count());
}

TEST_CASE("Icosphere normals are unit length")
{
    const auto mesh = make_icosphere(1);
    for (const auto& v : mesh.vertices()) {
        const float len = glm::length(v.normal);
        REQUIRE(len == Catch::Approx(1.0f).margin(1e-3f));
    }
}

TEST_CASE("Tangents are orthogonal to normals")
{
    const auto mesh = make_icosphere(1);
    for (const auto& v : mesh.vertices()) {
        const float d = glm::dot(glm::normalize(v.normal), glm::normalize(v.tangent));
        REQUIRE(std::abs(d) == Catch::Approx(0.0f).margin(1e-2f));
    }
}
