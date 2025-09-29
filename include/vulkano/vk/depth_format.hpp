#pragma once

#include <functional>
#include <vulkan/vulkan.h>

namespace vulkano::vk {
class DepthFormatResolver final {
public:
    using FormatQuery = std::function<VkFormatProperties(VkPhysicalDevice, VkFormat)>;

    [[nodiscard]] static VkFormat select_depth_format(VkPhysicalDevice physicalDevice);
    [[nodiscard]] static VkFormat select_depth_format(VkPhysicalDevice physicalDevice, const FormatQuery& query);
};
} // namespace vulkano::vk
