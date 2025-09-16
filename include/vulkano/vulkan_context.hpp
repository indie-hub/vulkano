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

private:
    void create_instance(GLFWwindow* window) noexcept;
    void setup_debug_utils() noexcept;
    void create_surface(GLFWwindow* window) noexcept;
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
};

} // namespace vulkano

