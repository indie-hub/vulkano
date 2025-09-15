#include <catch2/catch_all.hpp>

#include <vulkano/icosphere.hpp>
#include <vulkano/mesh.hpp>

#include <cmath>

static std::size_t expected_vertex_count(std::uint32_t subdivisions) {
    // V(n) = 10 * 4^n + 2
    std::size_t pow = 1;
    for (std::uint32_t i = 0; i < subdivisions; ++i) {
        pow *= 4U;
    }
    return 10ULL * pow + 2ULL;
}

static std::size_t expected_index_count(std::uint32_t subdivisions) {
    // F(n) = 20 * 4^n triangles, indices = F * 3
    std::size_t pow = 1;
    for (std::uint32_t i = 0; i < subdivisions; ++i) {
        pow *= 4U;
    }
    return (20ULL * pow) * 3ULL;
}

TEST_CASE("Icosphere counts match expected values") {
    vulkano::IcosphereBuilder builder;
    for (std::uint32_t sub = 0; sub <= 4; ++sub) {
        auto mesh = builder.build(sub);
        REQUIRE(mesh);
        REQUIRE(mesh->vertices.size() == expected_vertex_count(sub));
        REQUIRE(mesh->indices.size() == expected_index_count(sub));
    }
}

TEST_CASE("Icosphere normals are normalized") {
    vulkano::IcosphereBuilder builder;
    auto mesh = builder.build(2);
    for (const auto& v : mesh->vertices) {
        const float len = std::sqrt(v.normal[0]*v.normal[0] + v.normal[1]*v.normal[1] + v.normal[2]*v.normal[2]);
        REQUIRE(len == Catch::Approx(1.0F).margin(1e-3));
    }
}

