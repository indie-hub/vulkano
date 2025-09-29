#pragma once

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

namespace vulkano::vk {
struct QueueFamilySelection final {
    std::uint32_t graphicsFamily {0U};
    std::uint32_t presentFamily {0U};
};

struct PhysicalDeviceSelection final {
    VkPhysicalDevice device {VK_NULL_HANDLE};
    QueueFamilySelection queues {};
};

struct SwapchainSupport final {
    VkSurfaceCapabilitiesKHR capabilities {};
    std::vector<VkSurfaceFormatKHR> formats {};
    std::vector<VkPresentModeKHR> presentModes {};
};

class PhysicalDeviceSelector final {
public:
    static PhysicalDeviceSelection select(VkInstance instance, VkSurfaceKHR surface,
        const std::vector<const char*>& requiredExtensions);
};

SwapchainSupport query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface);
}
