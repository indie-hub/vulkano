#include <catch2/catch_all.hpp>

#include <vulkano/icosphere.hpp>
#include <vulkano/mesh.hpp>

#include <cmath>

static float length3(const float v[3]) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

TEST_CASE("Icosphere vertex count and normals") {
    vulkano::IcosphereBuilder builder {};

    for (std::uint32_t s = 0; s <= 3; ++s) {
        auto mesh = builder.build(s);
        REQUIRE(mesh != nullptr);
        // Faces grow by factor 4 each subdivision starting from 20 triangles
        const std::size_t tri_count = 20ULL * static_cast<std::size_t>(std::pow(4.0, static_cast<int>(s)));
        REQUIRE(mesh->indices.size() == tri_count * 3ULL);
        // Normals should be ~unit length
        for (const auto& v : mesh->vertices) {
            const float len = length3(v.normal);
            REQUIRE(len == Approx(1.0F).margin(1e-3F));
        }
    }
}

