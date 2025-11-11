#include <vulkano/vk/logical_device.hpp>

#include <set>
#include <stdexcept>
#include <vector>

namespace vulkano::vk {
LogicalDevice LogicalDevice::create(VkPhysicalDevice physicalDevice, const QueueFamilySelection& queues,
    bool enableValidationLayers, const std::vector<const char*>& validationLayers,
    const std::vector<const char*>& deviceExtensions) {
    std::set<std::uint32_t> uniqueQueueFamilies {
        queues.graphicsFamily,
        queues.presentFamily
    };

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(uniqueQueueFamilies.size());
    constexpr float queuePriority {1.0F};
    for (const std::uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1U;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0U;
        createInfo.ppEnabledLayerNames = nullptr;
    }

    VkDevice device {VK_NULL_HANDLE};
    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create logical device"};
    }

    VkQueue graphicsQueue {VK_NULL_HANDLE};
    vkGetDeviceQueue(device, queues.graphicsFamily, 0U, &graphicsQueue);

    VkQueue presentQueue {VK_NULL_HANDLE};
    vkGetDeviceQueue(device, queues.presentFamily, 0U, &presentQueue);

    return LogicalDevice {device, graphicsQueue, presentQueue};
}

LogicalDevice::LogicalDevice(VkDevice device, VkQueue graphicsQueue, VkQueue presentQueue) noexcept
    : m_device {device}
    , m_graphicsQueue {graphicsQueue}
    , m_presentQueue {presentQueue} {
}

LogicalDevice::LogicalDevice(LogicalDevice&& other) noexcept
    : m_device {other.m_device}
    , m_graphicsQueue {other.m_graphicsQueue}
    , m_presentQueue {other.m_presentQueue} {
    other.m_device = VK_NULL_HANDLE;
    other.m_graphicsQueue = VK_NULL_HANDLE;
    other.m_presentQueue = VK_NULL_HANDLE;
}

LogicalDevice& LogicalDevice::operator=(LogicalDevice&& other) noexcept {
    if (this != &other) {
        if (m_device != VK_NULL_HANDLE) {
            vkDestroyDevice(m_device, nullptr);
        }
        m_device = other.m_device;
        m_graphicsQueue = other.m_graphicsQueue;
        m_presentQueue = other.m_presentQueue;

        other.m_device = VK_NULL_HANDLE;
        other.m_graphicsQueue = VK_NULL_HANDLE;
        other.m_presentQueue = VK_NULL_HANDLE;
    }
    return *this;
}

LogicalDevice::~LogicalDevice() noexcept {
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
}

VkDevice LogicalDevice::handle() const noexcept {
    return m_device;
}

VkQueue LogicalDevice::graphics_queue() const noexcept {
    return m_graphicsQueue;
}

VkQueue LogicalDevice::present_queue() const noexcept {
    return m_presentQueue;
}

VkResult LogicalDevice::wait_idle() const noexcept {
    if (m_device == VK_NULL_HANDLE) {
        return VK_SUCCESS;
    }
    return vkDeviceWaitIdle(m_device);
}
} // namespace vulkano::vk
