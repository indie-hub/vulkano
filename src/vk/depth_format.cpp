#include <vulkano/vk/depth_format.hpp>

#include <array>
#include <stdexcept>

namespace {
constexpr std::array<VkFormat, 3> candidateFormats {
    VK_FORMAT_D32_SFLOAT,
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_D24_UNORM_S8_UINT
};
}

namespace vulkano::vk {
VkFormat DepthFormatResolver::select_depth_format(VkPhysicalDevice physicalDevice) {
    const FormatQuery query {[](VkPhysicalDevice device, VkFormat format) {
        VkFormatProperties properties {};
        vkGetPhysicalDeviceFormatProperties(device, format, &properties);
        return properties;
    }};
    return select_depth_format(physicalDevice, query);
}

VkFormat DepthFormatResolver::select_depth_format(VkPhysicalDevice physicalDevice, const FormatQuery& query) {
    for (VkFormat format : candidateFormats) {
        const VkFormatProperties properties = query(physicalDevice, format);
        if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0U) {
            return format;
        }
    }

    throw std::runtime_error {"Failed to find supported depth format"};
}
} // namespace vulkano::vk
