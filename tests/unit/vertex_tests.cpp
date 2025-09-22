#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vulkano/vertex.hpp>

using Catch::Approx;

TEST_CASE("Default triangle vertices cover expected positions", "[vertex]") {
    const auto vertices = vulkano::default_triangle_vertices();

    REQUIRE(vertices.size() == 3U);

    const auto& top = vertices[0].position;
    const auto& left = vertices[1].position;
    const auto& right = vertices[2].position;

    CHECK(top.x == Approx(0.0F));
    CHECK(top.y == Approx(0.5F));
    CHECK(top.z == Approx(0.0F));

    CHECK(left.x == Approx(-0.5F));
    CHECK(left.y == Approx(-0.5F));
    CHECK(left.z == Approx(0.0F));

    CHECK(right.x == Approx(0.5F));
    CHECK(right.y == Approx(-0.5F));
    CHECK(right.z == Approx(0.0F));
}
