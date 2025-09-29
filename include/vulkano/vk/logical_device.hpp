#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include <vulkano/vk/physical_device.hpp>

namespace vulkano::vk {
class LogicalDevice final {
public:
    LogicalDevice() noexcept = default;
    static LogicalDevice create(VkPhysicalDevice physicalDevice, const QueueFamilySelection& queues,
        bool enableValidationLayers, const std::vector<const char*>& validationLayers,
        const std::vector<const char*>& deviceExtensions);

    LogicalDevice(const LogicalDevice&) = delete;
    LogicalDevice& operator=(const LogicalDevice&) = delete;
    LogicalDevice(LogicalDevice&& other) noexcept;
    LogicalDevice& operator=(LogicalDevice&& other) noexcept;
    ~LogicalDevice() noexcept;

    [[nodiscard]] VkDevice handle() const noexcept;
    [[nodiscard]] VkQueue graphics_queue() const noexcept;
    [[nodiscard]] VkQueue present_queue() const noexcept;
    [[nodiscard]] VkResult wait_idle() const noexcept;

private:
    LogicalDevice(VkDevice device, VkQueue graphicsQueue, VkQueue presentQueue) noexcept;

    VkDevice m_device {VK_NULL_HANDLE};
    VkQueue m_graphicsQueue {VK_NULL_HANDLE};
    VkQueue m_presentQueue {VK_NULL_HANDLE};
};
} // namespace vulkano::vk
