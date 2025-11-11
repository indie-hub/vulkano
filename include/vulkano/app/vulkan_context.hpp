#pragma once

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

#include <vulkano/vk/instance.hpp>
#include <vulkano/vk/logical_device.hpp>
#include <vulkano/vk/swapchain.hpp>

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
    void recreate_swapchain(const Window& window);

    [[nodiscard]] VkInstance instance() const noexcept;
    [[nodiscard]] VkPhysicalDevice physical_device() const noexcept;
    [[nodiscard]] VkDevice device() const noexcept;
    [[nodiscard]] VkSurfaceKHR surface() const noexcept;
    [[nodiscard]] VkQueue graphics_queue() const noexcept;
    [[nodiscard]] VkQueue present_queue() const noexcept;
    [[nodiscard]] std::uint32_t graphics_queue_family_index() const noexcept;
    [[nodiscard]] std::uint32_t present_queue_family_index() const noexcept;
    [[nodiscard]] VkSwapchainKHR swapchain() const noexcept;
    [[nodiscard]] VkFormat swapchain_image_format() const noexcept;
    [[nodiscard]] VkExtent2D swapchain_extent() const noexcept;
    [[nodiscard]] const std::vector<VkImageView>& swapchain_image_views() const noexcept;

private:
    vk::Instance m_instance {};
    VkSurfaceKHR m_surface {VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice {VK_NULL_HANDLE};
    vk::LogicalDevice m_device {};
    std::uint32_t m_graphicsQueueFamilyIndex {0U};
    std::uint32_t m_presentQueueFamilyIndex {0U};
    vk::SwapchainManager m_swapchain {};
};
} // namespace vulkano::app
