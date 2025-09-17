// primitive_state_tests.cpp
#include <catch2/catch_all.hpp>

#include <vulkano/geometry.hpp>

TEST_CASE("Per-primitive transforms are independent") {
    using namespace vulkano;
    Plane a {};
    Cube b {};

    Transform tA {};
    tA.position = glm::vec3 {1.0F, 2.0F, 3.0F};
    tA.rotationEuler = glm::vec3 {0.1F, 0.2F, 0.3F};
    tA.scale = glm::vec3 {1.0F, 2.0F, 3.0F};

    Transform tB {};
    tB.position = glm::vec3 {-1.0F, -2.0F, -3.0F};
    tB.rotationEuler = glm::vec3 {0.0F, 0.0F, 0.0F};
    tB.scale = glm::vec3 {0.5F, 0.5F, 0.5F};

    a.set_transform(tA);
    b.set_transform(tB);

    REQUIRE(a.transform().position != b.transform().position);
    REQUIRE(a.transform().rotationEuler != b.transform().rotationEuler);
    REQUIRE(a.transform().scale != b.transform().scale);

    // Mutate A and ensure B unchanged
    tA.position = glm::vec3 {10.0F, 0.0F, 0.0F};
    a.set_transform(tA);
    REQUIRE(a.transform().position == glm::vec3 {10.0F, 0.0F, 0.0F});
    REQUIRE(b.transform().position == glm::vec3 {-1.0F, -2.0F, -3.0F});
}

TEST_CASE("Per-primitive materials are independent") {
    using namespace vulkano;
    Plane a {};
    Cube b {};

    Material mA {};
    mA.baseColor = glm::vec3 {0.2F, 0.4F, 0.6F};
    mA.shininess = 8.0F;
    mA.useAlbedo = true;
    mA.useNormal = false;
    mA.normalStrength = 0.5F;

    Material mB {};
    mB.baseColor = glm::vec3 {0.8F, 0.6F, 0.4F};
    mB.shininess = 128.0F;
    mB.useAlbedo = false;
    mB.useNormal = true;
    mB.normalStrength = 1.5F;

    a.set_material(mA);
    b.set_material(mB);

    REQUIRE(a.material().baseColor != b.material().baseColor);
    REQUIRE(a.material().shininess != b.material().shininess);
    REQUIRE(a.material().useAlbedo != b.material().useAlbedo);
    REQUIRE(a.material().useNormal != b.material().useNormal);
    REQUIRE(a.material().normalStrength != b.material().normalStrength);

    // Mutate B and ensure A unchanged
    mB.baseColor = glm::vec3 {1.0F, 1.0F, 1.0F};
    mB.shininess = 4.0F;
    mB.useAlbedo = true;
    mB.useNormal = false;
    mB.normalStrength = 0.0F;
    b.set_material(mB);

    REQUIRE(b.material().baseColor == glm::vec3 {1.0F, 1.0F, 1.0F});
    REQUIRE(b.material().shininess == Catch::Approx(4.0F));
    REQUIRE(b.material().useAlbedo == true);
    REQUIRE(b.material().useNormal == false);
    REQUIRE(b.material().normalStrength == Catch::Approx(0.0F));

    REQUIRE(a.material().baseColor == glm::vec3 {0.2F, 0.4F, 0.6F});
    REQUIRE(a.material().shininess == Catch::Approx(8.0F));
    REQUIRE(a.material().useAlbedo == true);
    REQUIRE(a.material().useNormal == false);
    REQUIRE(a.material().normalStrength == Catch::Approx(0.5F));
}

