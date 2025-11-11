#include <vulkano/vk/physical_device.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {
struct QueueFamilyDiscovery final {
    std::optional<std::uint32_t> graphicsFamily {};
    std::optional<std::uint32_t> presentFamily {};

    [[nodiscard]] bool complete() const noexcept {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

QueueFamilyDiscovery find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyDiscovery indices {};

    std::uint32_t queueFamilyCount {0U};
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (std::uint32_t i {0U}; i < queueFamilyCount; ++i) {
        const VkQueueFamilyProperties& queueFamily = queueFamilies[i];
        if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
            indices.graphicsFamily = i;
        }
        VkBool32 presentSupport {VK_FALSE};
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport == VK_TRUE) {
            indices.presentFamily = i;
        }
        if (indices.complete()) {
            break;
        }
    }

    return indices;
}

bool check_device_extension_support(VkPhysicalDevice device, const std::vector<const char*>& extensions) {
    std::uint32_t extensionCount {0U};
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    for (const char* required : extensions) {
        const bool found = std::any_of(availableExtensions.begin(), availableExtensions.end(),
            [required](const VkExtensionProperties& property) {
                return std::strcmp(required, property.extensionName) == 0;
            });
        if (!found) {
            return false;
        }
    }
    return true;
}

bool is_device_suitable(VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& extensions) {
    const QueueFamilyDiscovery families = find_queue_families(device, surface);
    if (!families.complete()) {
        return false;
    }

    if (!check_device_extension_support(device, extensions)) {
        return false;
    }

    const vulkano::vk::SwapchainSupport support = vulkano::vk::query_swapchain_support(device, surface);
    if (support.formats.empty() || support.presentModes.empty()) {
        return false;
    }

    VkPhysicalDeviceFeatures supportedFeatures {};
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);
    return supportedFeatures.samplerAnisotropy == VK_TRUE;
}
}

namespace vulkano::vk {
SwapchainSupport query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapchainSupport details {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    std::uint32_t formatCount {0U};
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    details.formats.resize(formatCount);
    if (formatCount != 0U) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    std::uint32_t presentModeCount {0U};
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    details.presentModes.resize(presentModeCount);
    if (presentModeCount != 0U) {
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

PhysicalDeviceSelection PhysicalDeviceSelector::select(VkInstance instance, VkSurfaceKHR surface,
    const std::vector<const char*>& requiredExtensions) {
    std::uint32_t deviceCount {0U};
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0U) {
        throw std::runtime_error {"Failed to find GPUs with Vulkan support"};
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (VkPhysicalDevice device : devices) {
        if (is_device_suitable(device, surface, requiredExtensions)) {
            const QueueFamilyDiscovery families = find_queue_families(device, surface);
            return PhysicalDeviceSelection {
                .device = device,
                .queues = QueueFamilySelection {
                    .graphicsFamily = families.graphicsFamily.value(),
                    .presentFamily = families.presentFamily.value() }
            };
        }
    }

    throw std::runtime_error {"Failed to find a suitable GPU"};
}
} // namespace vulkano::vk
