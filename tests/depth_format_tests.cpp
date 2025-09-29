#include <catch2/catch_test_macros.hpp>

#include <vulkano/vk/depth_format.hpp>

#include <unordered_map>
#include <stdexcept>

namespace {
vulkano::vk::DepthFormatResolver::FormatQuery make_query(
    const std::unordered_map<VkFormat, VkFormatFeatureFlags>& support) {
    return vulkano::vk::DepthFormatResolver::FormatQuery {
        [support](VkPhysicalDevice, VkFormat format) {
            VkFormatProperties properties {};
            const auto it = support.find(format);
            if (it != support.end()) {
                properties.optimalTilingFeatures = it->second;
            }
            return properties;
        }
    };
}
} // namespace

TEST_CASE("DepthFormatResolver selects preferred supported format") {
    const auto query = make_query({{VK_FORMAT_D32_SFLOAT, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT}});
    const VkFormat format = vulkano::vk::DepthFormatResolver::select_depth_format(VK_NULL_HANDLE, query);
    REQUIRE(format == VK_FORMAT_D32_SFLOAT);
}

TEST_CASE("DepthFormatResolver falls back when preferred format unsupported") {
    const auto query = make_query({{VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT}});
    const VkFormat format = vulkano::vk::DepthFormatResolver::select_depth_format(VK_NULL_HANDLE, query);
    REQUIRE(format == VK_FORMAT_D32_SFLOAT_S8_UINT);
}

TEST_CASE("DepthFormatResolver uses final fallback when necessary") {
    const auto query = make_query({{VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT}});
    const VkFormat format = vulkano::vk::DepthFormatResolver::select_depth_format(VK_NULL_HANDLE, query);
    REQUIRE(format == VK_FORMAT_D24_UNORM_S8_UINT);
}

TEST_CASE("DepthFormatResolver throws when no formats supported") {
    const auto query = make_query({});
    REQUIRE_THROWS_AS((void)vulkano::vk::DepthFormatResolver::select_depth_format(VK_NULL_HANDLE, query),
        std::runtime_error);
}
