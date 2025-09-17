#include <catch2/catch_all.hpp>

#include <vulkano/geometry.hpp>

TEST_CASE("Plane generates 4 verts and 6 indices") {
    const vulkano::Plane::Params p {1.0F, 2.0F, {1.0F, 1.0F}};
    const vulkano::Plane plane {p};
    REQUIRE(plane.vertices().size() == 4);
    REQUIRE(plane.indices().size() == 6);
}

TEST_CASE("Cube generates 24 verts and 36 indices") {
    const vulkano::Cube cube {};
    REQUIRE(cube.vertices().size() == 24);
    REQUIRE(cube.indices().size() == 36);
}

TEST_CASE("Icosphere vertex/index counts grow with subdivisions") {
    const vulkano::Icosphere s0 {{0U}};
    const vulkano::Icosphere s1 {{1U}};
    const vulkano::Icosphere s2 {{2U}};
    // Basic monotonic checks
    REQUIRE(s0.vertices().size() < s1.vertices().size());
    REQUIRE(s1.vertices().size() < s2.vertices().size());
    REQUIRE(s0.indices().size() < s1.indices().size());
    REQUIRE(s1.indices().size() < s2.indices().size());
}

TEST_CASE("Icosphere set_subdivisions rebuilds geometry") {
    vulkano::Icosphere s {{0U}};
    const auto v0 = s.vertices().size();
    const auto i0 = s.indices().size();
    s.set_subdivisions(2U);
    const auto v1 = s.vertices().size();
    const auto i1 = s.indices().size();
    REQUIRE(v1 > v0);
    REQUIRE(i1 > i0);
}

TEST_CASE("Plane UV tiling setter updates UVs") {
    using namespace vulkano;
    Plane::Params p {};
    p.width = 2.0F;
    p.depth = 2.0F;
    p.uvTiling = glm::vec2 {1.0F, 1.0F};
    Plane plane {p};
    REQUIRE(plane.vertices().size() == 4);
    const auto uv_before = plane.vertices()[1].uv; // top-right
    REQUIRE(uv_before.x == Catch::Approx(1.0F));
    REQUIRE(uv_before.y == Catch::Approx(0.0F));
    plane.set_uv_tiling(glm::vec2 {2.0F, 3.0F});
    REQUIRE(plane.vertices().size() == 4);
    const auto uv_after = plane.vertices()[1].uv;
    REQUIRE(uv_after.x == Catch::Approx(2.0F));
    REQUIRE(uv_after.y == Catch::Approx(0.0F));
}
