#pragma once

#include <cstdint>
#include <vector>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

namespace vulkano {

class VulkanContext final {
public:
    explicit VulkanContext(GLFWwindow* window) noexcept;
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&) = delete;
    VulkanContext& operator=(VulkanContext&&) = delete;
    ~VulkanContext() noexcept;

    [[nodiscard]] VkInstance instance() const noexcept;
    [[nodiscard]] VkSurfaceKHR surface() const noexcept;
    [[nodiscard]] bool validation_enabled() const noexcept;

    // Device accessors
    [[nodiscard]] VkPhysicalDevice physical_device() const noexcept;
    [[nodiscard]] VkDevice device() const noexcept;
    [[nodiscard]] VkQueue graphics_queue() const noexcept;
    [[nodiscard]] VkQueue present_queue() const noexcept;
    [[nodiscard]] std::uint32_t graphics_queue_family() const noexcept;
    [[nodiscard]] std::uint32_t present_queue_family() const noexcept;

    // Swapchain accessors
    [[nodiscard]] VkFormat swapchain_image_format() const noexcept;
    [[nodiscard]] VkExtent2D swapchain_extent() const noexcept;

private:
    void create_instance(GLFWwindow* window) noexcept;
    void setup_debug_utils() noexcept;
    void create_surface(GLFWwindow* window) noexcept;
    void pick_physical_device() noexcept;
    void create_logical_device() noexcept;
    void create_swapchain_and_views(GLFWwindow* window) noexcept;
    void destroy_swapchain_and_views() noexcept;
    void destroy() noexcept;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) noexcept;

private:
    VkInstance instance_ {VK_NULL_HANDLE};
    VkSurfaceKHR surface_ {VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT debug_messenger_ {VK_NULL_HANDLE};
    bool validation_enabled_ {false};

    // Device and queues
    VkPhysicalDevice physical_device_ {VK_NULL_HANDLE};
    VkDevice device_ {VK_NULL_HANDLE};
    VkQueue graphics_queue_ {VK_NULL_HANDLE};
    VkQueue present_queue_ {VK_NULL_HANDLE};
    std::uint32_t graphics_family_index_ {UINT32_MAX};
    std::uint32_t present_family_index_ {UINT32_MAX};

    // Swapchain
    VkSwapchainKHR swapchain_ {VK_NULL_HANDLE};
    VkFormat swapchain_image_format_ {VK_FORMAT_UNDEFINED};
    VkExtent2D swapchain_extent_ {0U, 0U};
    std::vector<VkImage> swapchain_images_ {};
    std::vector<VkImageView> swapchain_image_views_ {};
};

} // namespace vulkano
