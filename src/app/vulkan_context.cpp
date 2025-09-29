#include <vulkano/app/vulkan_context.hpp>

#include <vulkano/app/window.hpp>
#include <vulkano/vk/instance.hpp>
#include <vulkano/vk/physical_device.hpp>

#include <GLFW/glfw3.h>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::vector<const char*> required_device_extensions() {
    std::vector<const char*> extensions {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#ifdef __APPLE__
    extensions.push_back("VK_KHR_portability_subset");
#endif
    return extensions;
}

VkSurfaceKHR create_surface(const vulkano::vk::Instance& instance, const vulkano::app::Window& window) {
    VkSurfaceKHR surface {VK_NULL_HANDLE};
    if (glfwCreateWindowSurface(instance.handle(), window.handle(), nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create window surface"};
    }
    return surface;
}

void log_vulkan_error(const std::string& context, VkResult result) noexcept {
    if (result != VK_SUCCESS) {
        std::cerr << context << " failed with error code " << result << '\n';
    }
}
} // namespace

namespace vulkano::app {
VulkanContext::VulkanContext(const Window& window) {
    m_instance = vk::Instance::create();
    m_surface = create_surface(m_instance, window);

    const std::vector<const char*> deviceExtensions = required_device_extensions();
    const vk::PhysicalDeviceSelection selection = vk::PhysicalDeviceSelector::select(m_instance.handle(), m_surface, deviceExtensions);
    m_physicalDevice = selection.device;
    m_graphicsQueueFamilyIndex = selection.queues.graphicsFamily;
    m_presentQueueFamilyIndex = selection.queues.presentFamily;

    m_device = vk::LogicalDevice::create(m_physicalDevice, selection.queues, m_instance.validation_layers_enabled(),
        m_instance.validation_layers(), deviceExtensions);

    m_swapchain = vk::SwapchainManager::create(m_physicalDevice, m_device.handle(), m_surface, selection.queues, window);
}

VulkanContext::~VulkanContext() noexcept {
    const VkResult waitResult = m_device.wait_idle();
    log_vulkan_error("vkDeviceWaitIdle", waitResult);

    m_swapchain = vk::SwapchainManager {};

    if (m_surface != VK_NULL_HANDLE && m_instance.handle() != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance.handle(), m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
}

void VulkanContext::wait_idle() const noexcept {
    const VkResult result = m_device.wait_idle();
    log_vulkan_error("vkDeviceWaitIdle", result);
}

VkInstance VulkanContext::instance() const noexcept {
    return m_instance.handle();
}

VkPhysicalDevice VulkanContext::physical_device() const noexcept {
    return m_physicalDevice;
}

VkDevice VulkanContext::device() const noexcept {
    return m_device.handle();
}

VkSurfaceKHR VulkanContext::surface() const noexcept {
    return m_surface;
}

VkQueue VulkanContext::graphics_queue() const noexcept {
    return m_device.graphics_queue();
}

VkQueue VulkanContext::present_queue() const noexcept {
    return m_device.present_queue();
}

std::uint32_t VulkanContext::graphics_queue_family_index() const noexcept {
    return m_graphicsQueueFamilyIndex;
}

std::uint32_t VulkanContext::present_queue_family_index() const noexcept {
    return m_presentQueueFamilyIndex;
}

VkSwapchainKHR VulkanContext::swapchain() const noexcept {
    return m_swapchain.handle();
}

VkFormat VulkanContext::swapchain_image_format() const noexcept {
    return m_swapchain.image_format();
}

VkExtent2D VulkanContext::swapchain_extent() const noexcept {
    return m_swapchain.extent();
}

const std::vector<VkImageView>& VulkanContext::swapchain_image_views() const noexcept {
    return m_swapchain.image_views();
}
} // namespace vulkano::app
