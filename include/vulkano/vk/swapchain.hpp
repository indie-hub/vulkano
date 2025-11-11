#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include <vulkano/vk/physical_device.hpp>

namespace vulkano {
namespace app {
class Window;
} // namespace app
} // namespace vulkano

namespace vulkano::vk {
class SwapchainManager final {
public:
    SwapchainManager() noexcept = default;
    static SwapchainManager create(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
        const QueueFamilySelection& queues, const app::Window& window);

    SwapchainManager(const SwapchainManager&) = delete;
    SwapchainManager& operator=(const SwapchainManager&) = delete;
    SwapchainManager(SwapchainManager&& other) noexcept;
    SwapchainManager& operator=(SwapchainManager&& other) noexcept;
    ~SwapchainManager() noexcept;

    [[nodiscard]] VkSwapchainKHR handle() const noexcept;
    [[nodiscard]] VkFormat image_format() const noexcept;
    [[nodiscard]] VkExtent2D extent() const noexcept;
    [[nodiscard]] const std::vector<VkImage>& images() const noexcept;
    [[nodiscard]] const std::vector<VkImageView>& image_views() const noexcept;

private:
    SwapchainManager(VkDevice device, VkSwapchainKHR swapchain, VkFormat format, VkExtent2D extent,
        std::vector<VkImage> images, std::vector<VkImageView> imageViews) noexcept;

    VkDevice m_device {VK_NULL_HANDLE};
    VkSwapchainKHR m_swapchain {VK_NULL_HANDLE};
    VkFormat m_format {VK_FORMAT_UNDEFINED};
    VkExtent2D m_extent {0U, 0U};
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
};
} // namespace vulkano::vk
