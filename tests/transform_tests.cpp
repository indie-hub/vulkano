#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vulkano/scene/transform.hpp>

namespace {
void expect_matrix_close(const glm::mat4& actual, const glm::mat4& expected, float epsilon) {
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            INFO("Mismatch at [" << column << "][" << row << "]");
            REQUIRE(actual[column][row] == Catch::Approx(expected[column][row]).margin(epsilon));
        }
    }
}
} // namespace

TEST_CASE("Transform::matrix composes translate rotate scale") {
    vulkano::scene::Transform transform {};
    transform.position = glm::vec3 {2.0F, 3.0F, -4.0F};
    transform.rotation = glm::angleAxis(glm::half_pi<float>(), glm::vec3 {0.0F, 1.0F, 0.0F});
    transform.scale = glm::vec3 {1.0F, 2.0F, 3.0F};

    const glm::mat4 expected = glm::translate(glm::mat4 {1.0F}, transform.position)
        * glm::mat4_cast(transform.rotation) * glm::scale(glm::mat4 {1.0F}, transform.scale);

    expect_matrix_close(transform.matrix(), expected, 1e-5F);
}

TEST_CASE("Transform::from_matrix decomposes TRS matrix") {
    const glm::vec3 position {1.0F, -2.0F, 3.5F};
    const glm::vec3 scale {2.0F, 0.5F, -1.5F};
    const glm::quat rotation = glm::angleAxis(glm::radians(45.0F), glm::vec3 {0.0F, 1.0F, 0.0F});

    glm::mat4 matrix = glm::translate(glm::mat4 {1.0F}, position);
    matrix *= glm::mat4_cast(rotation);
    matrix *= glm::scale(glm::mat4 {1.0F}, scale);

    const vulkano::scene::Transform transform = vulkano::scene::Transform::from_matrix(matrix);

    REQUIRE(transform.position.x == Catch::Approx(position.x).margin(1e-4F));
    REQUIRE(transform.position.y == Catch::Approx(position.y).margin(1e-4F));
    REQUIRE(transform.position.z == Catch::Approx(position.z).margin(1e-4F));
    REQUIRE(std::abs(transform.scale.x) == Catch::Approx(std::abs(scale.x)).margin(1e-4F));
    REQUIRE(std::abs(transform.scale.y) == Catch::Approx(std::abs(scale.y)).margin(1e-4F));
    REQUIRE(std::abs(transform.scale.z) == Catch::Approx(std::abs(scale.z)).margin(1e-4F));
    const glm::mat4 recomposed = transform.matrix();
    expect_matrix_close(recomposed, matrix, 1e-4F);
}

TEST_CASE("Transform euler angle helpers round-trip") {
    vulkano::scene::Transform transform {};
    const glm::vec3 expectedDegrees {10.0F, 25.0F, -40.0F};
    transform.set_euler_degrees(expectedDegrees);
    const glm::vec3 actualDegrees = transform.euler_degrees();
    REQUIRE(actualDegrees.x == Catch::Approx(expectedDegrees.x).margin(1e-3F));
    REQUIRE(actualDegrees.y == Catch::Approx(expectedDegrees.y).margin(1e-3F));
    REQUIRE(actualDegrees.z == Catch::Approx(expectedDegrees.z).margin(1e-3F));
}
