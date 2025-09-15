#pragma once

#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace vulkano {

class VulkanContext final {
public:
    VulkanContext();
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    void init_window(int width, int height, const std::string& title);
    void init_vulkan();
    void cleanup();

    GLFWwindow* window() const noexcept;
    void poll_events() noexcept;
    bool should_close() const noexcept;
    VkInstance instance() const noexcept;
    VkPhysicalDevice physical_device() const noexcept;
    VkDevice device() const noexcept;
    VkQueue graphics_queue() const noexcept;
    VkQueue present_queue() const noexcept;
    VkSurfaceKHR surface() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl {};
};

} // namespace vulkano
