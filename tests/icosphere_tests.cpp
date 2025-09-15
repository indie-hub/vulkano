#include <catch2/catch_all.hpp>

#include <vulkan_app/icosphere.hpp>

TEST_CASE("Icosphere vertex and index counts", "[icosphere]") {
    const auto mesh0 {vulkan_app::make_icosphere(0)};
    REQUIRE(mesh0.vertices.size() == 12);
    REQUIRE(mesh0.indices.size() == 60);

    const auto mesh1 {vulkan_app::make_icosphere(1)};
    REQUIRE(mesh1.vertices.size() == 42);
    REQUIRE(mesh1.indices.size() == 240);
}

TEST_CASE("Icosphere TBN correctness", "[icosphere]") {
    const auto mesh {vulkan_app::make_icosphere(2)};
    // Spot-check a subset of vertices for normalized normal and orthonormal TBN
    for (std::size_t i {0}; i < mesh.vertices.size(); i += 17) {
        const auto &v = mesh.vertices[i];
        const float nlen = glm::length(v.normal);
        REQUIRE(nlen == Catch::Approx(1.0F).margin(1e-3F));
        const float tlen = glm::length(v.tangent);
        const float blen = glm::length(v.bitangent);
        REQUIRE(tlen == Catch::Approx(1.0F).margin(1e-3F));
        REQUIRE(blen == Catch::Approx(1.0F).margin(1e-3F));
        const float nt = glm::dot(glm::normalize(v.normal), glm::normalize(v.tangent));
        const float nb = glm::dot(glm::normalize(v.normal), glm::normalize(v.bitangent));
        REQUIRE(std::fabs(nt) <= 1e-2F);
        REQUIRE(std::fabs(nb) <= 1e-2F);
    }
}
