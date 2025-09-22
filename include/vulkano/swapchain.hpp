#pragma once

#include <vulkano/vulkan_context.hpp>

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

namespace vulkano {

class Swapchain final {
public:
    Swapchain() = default;
    Swapchain(const Swapchain&) = delete;
    Swapchain(Swapchain&& other) noexcept;
    auto operator=(const Swapchain&) -> Swapchain& = delete;
    auto operator=(Swapchain&& other) noexcept -> Swapchain&;
    ~Swapchain() noexcept;

    [[nodiscard]] static auto create(const VulkanContext& context, const Window& window) -> Swapchain;

    [[nodiscard]] auto handle() const noexcept -> VkSwapchainKHR;
    [[nodiscard]] auto image_format() const noexcept -> VkFormat;
    [[nodiscard]] auto extent() const noexcept -> VkExtent2D;
    [[nodiscard]] auto image_views() const noexcept -> const std::vector<VkImageView>&;

    void recreate(const VulkanContext& context, const Window& window);

private:
    friend class SwapchainBuilder;
    Swapchain(const VulkanContext& context, const Window& window);

    void initialise(const VulkanContext& context, const Window& window);
    void cleanup() noexcept;
    void move_from(Swapchain&& other) noexcept;

    VkSwapchainKHR m_swapchain {VK_NULL_HANDLE};
    VkDevice m_device {VK_NULL_HANDLE};
    VkFormat m_imageFormat {VK_FORMAT_UNDEFINED};
    VkExtent2D m_extent {0U, 0U};
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
};

class SwapchainBuilder final {
public:
    SwapchainBuilder();

    [[nodiscard]] auto with_context(const VulkanContext& context) -> SwapchainBuilder&;
    [[nodiscard]] auto with_window(const Window& window) -> SwapchainBuilder&;
    [[nodiscard]] auto build() const -> Swapchain;

private:
    const VulkanContext* m_context;
    const Window* m_window;
};

} // namespace vulkano
