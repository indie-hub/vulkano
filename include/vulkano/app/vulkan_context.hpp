#pragma once

#include <vector>
#include <vulkan/vulkan.h>

namespace vulkano::app {
class Window;

class VulkanContext final {
public:
    explicit VulkanContext(const Window& window);
    ~VulkanContext() noexcept;

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&) noexcept = delete;
    VulkanContext& operator=(VulkanContext&&) noexcept = delete;

    void wait_idle() const noexcept;

    [[nodiscard]] VkInstance instance() const noexcept;
    [[nodiscard]] VkDevice device() const noexcept;
    [[nodiscard]] VkSurfaceKHR surface() const noexcept;
    [[nodiscard]] VkQueue graphics_queue() const noexcept;
    [[nodiscard]] VkQueue present_queue() const noexcept;
    [[nodiscard]] VkSwapchainKHR swapchain() const noexcept;
    [[nodiscard]] VkFormat swapchain_image_format() const noexcept;
    [[nodiscard]] VkExtent2D swapchain_extent() const noexcept;
    [[nodiscard]] const std::vector<VkImageView>& swapchain_image_views() const noexcept;

private:
    VkInstance m_instance {VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT m_debugMessenger {VK_NULL_HANDLE};
    VkSurfaceKHR m_surface {VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice {VK_NULL_HANDLE};
    VkDevice m_device {VK_NULL_HANDLE};
    VkQueue m_graphicsQueue {VK_NULL_HANDLE};
    VkQueue m_presentQueue {VK_NULL_HANDLE};
    VkSwapchainKHR m_swapchain {VK_NULL_HANDLE};
    VkFormat m_swapchainFormat {VK_FORMAT_UNDEFINED};
    VkExtent2D m_swapchainExtent {0U, 0U};
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
};
} // namespace vulkano::app
