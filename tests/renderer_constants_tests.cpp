#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include <vulkano/app/scene_renderer.hpp>

TEST_CASE("SceneRenderer reports expected color attachment count") {
    constexpr std::uint32_t expectedAttachments {4U};
    REQUIRE(vulkano::app::SceneRenderer::color_attachment_count() == expectedAttachments);
}
