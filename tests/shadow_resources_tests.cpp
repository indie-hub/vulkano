#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstddef>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec3.hpp>

#include <vulkano/app/shadow_resources.hpp>

namespace {
constexpr float epsilon = 1e-4F;

void expect_matrix_eq(const glm::mat4& actual, const glm::mat4& expected) {
    using Catch::Matchers::WithinAbs;
    const float* actualPtr = glm::value_ptr(actual);
    const float* expectedPtr = glm::value_ptr(expected);
    for (int i = 0; i < 16; ++i) {
        REQUIRE_THAT(actualPtr[i], WithinAbs(expectedPtr[i], epsilon));
    }
}
} // namespace

TEST_CASE("ShadowResources matrix payload writes matrices and metadata") {
    vulkano::app::ShadowResources resources {};
    resources.slots.resize(2U);

    resources.slots[0].viewProjection = glm::mat4(1.0F);
    resources.slots[1].viewProjection = glm::translate(glm::mat4(1.0F), glm::vec3 {1.0F, 2.0F, 3.0F});
    resources.activeCount = 1U;

    const VkDeviceSize payloadSize = resources.matrix_payload_size();
    REQUIRE(payloadSize == sizeof(glm::mat4) * 2U + sizeof(glm::uvec4));

    std::vector<std::byte> payload(static_cast<std::size_t>(payloadSize));
    resources.write_matrix_payload(payload.data());

    const auto* matrices = reinterpret_cast<const glm::mat4*>(payload.data());
    expect_matrix_eq(matrices[0], resources.slots[0].viewProjection);
    expect_matrix_eq(matrices[1], resources.slots[1].viewProjection);

    const auto* metadata = reinterpret_cast<const glm::uvec4*>(matrices + resources.slots.size());
    REQUIRE(metadata->x == resources.activeCount);
    REQUIRE(metadata->y == 0U);
    REQUIRE(metadata->z == 0U);
    REQUIRE(metadata->w == 0U);
}

TEST_CASE("ShadowResources payload handles empty slot list") {
    vulkano::app::ShadowResources resources {};
    resources.activeCount = 0U;

    const VkDeviceSize payloadSize = resources.matrix_payload_size();
    REQUIRE(payloadSize == sizeof(glm::uvec4));

    std::vector<std::byte> payload(static_cast<std::size_t>(payloadSize), std::byte {0xFF});
    resources.write_matrix_payload(payload.data());

    const auto* metadata = reinterpret_cast<const glm::uvec4*>(payload.data());
    REQUIRE(metadata->x == resources.activeCount);
    REQUIRE(metadata->y == 0U);
    REQUIRE(metadata->z == 0U);
    REQUIRE(metadata->w == 0U);
}
