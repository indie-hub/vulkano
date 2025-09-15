#include <vulkano/vulkan_context.hpp>

#include <stdexcept>
#include <string>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace vulkano {

struct VulkanContext::Impl {
    GLFWwindow* window {nullptr};
    VkInstance instance {VK_NULL_HANDLE};
    VkSurfaceKHR surface {VK_NULL_HANDLE};
    VkPhysicalDevice physical_device {VK_NULL_HANDLE};
    VkDevice device {VK_NULL_HANDLE};
    VkQueue graphics_queue {VK_NULL_HANDLE};
    VkQueue present_queue {VK_NULL_HANDLE};
};

VulkanContext::VulkanContext()
    : m_impl {std::make_unique<Impl>()}
{
}

VulkanContext::~VulkanContext()
{
    cleanup();
}

void VulkanContext::init_window(int width, int height, const std::string& title)
{
    if (!glfwInit()) {
        throw std::runtime_error("Failed to init GLFW");
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    m_impl->window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_impl->window) {
        throw std::runtime_error("Failed to create GLFW window");
    }
}

void VulkanContext::init_vulkan()
{
    // Minimal instance creation (no validation for stub)
    VkApplicationInfo app_info {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app_info.pApplicationName = "VulkanoCodex";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "Vulkano";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    uint32_t glfw_ext_count = 0;
    const char** glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

    VkInstanceCreateInfo create_info {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = glfw_ext_count;
    create_info.ppEnabledExtensionNames = glfw_exts;

    if (vkCreateInstance(&create_info, nullptr, &m_impl->instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    if (glfwCreateWindowSurface(m_impl->instance, m_impl->window, nullptr, &m_impl->surface) != GLFW_TRUE) {
        throw std::runtime_error("Failed to create window surface");
    }

    // Device selection stub: pick first available
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(m_impl->instance, &device_count, nullptr);
    if (device_count == 0) {
        throw std::runtime_error("No Vulkan physical devices found");
    }
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(m_impl->instance, &device_count, devices.data());
    m_impl->physical_device = devices[0];

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_info.queueFamilyIndex = 0; // Stub: assume queue family 0 supports graphics+present on most platforms
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_priority;

    VkDeviceCreateInfo dev_info {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dev_info.queueCreateInfoCount = 1;
    dev_info.pQueueCreateInfos = &queue_info;
    const char* dev_exts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    dev_info.enabledExtensionCount = 1;
    dev_info.ppEnabledExtensionNames = dev_exts;
    if (vkCreateDevice(m_impl->physical_device, &dev_info, nullptr, &m_impl->device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }
    vkGetDeviceQueue(m_impl->device, 0, 0, &m_impl->graphics_queue);
    m_impl->present_queue = m_impl->graphics_queue;
}

void VulkanContext::cleanup()
{
    if (m_impl) {
        if (m_impl->device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_impl->device);
            vkDestroyDevice(m_impl->device, nullptr);
            m_impl->device = VK_NULL_HANDLE;
        }
        if (m_impl->surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(m_impl->instance, m_impl->surface, nullptr);
            m_impl->surface = VK_NULL_HANDLE;
        }
        if (m_impl->instance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_impl->instance, nullptr);
            m_impl->instance = VK_NULL_HANDLE;
        }
        if (m_impl->window) {
            glfwDestroyWindow(m_impl->window);
            glfwTerminate();
            m_impl->window = nullptr;
        }
    }
}

GLFWwindow* VulkanContext::window() const noexcept
{
    return m_impl->window;
}

void VulkanContext::poll_events() noexcept
{
    glfwPollEvents();
}

bool VulkanContext::should_close() const noexcept
{
    if (!m_impl->window) {
        return true;
    }
    return glfwWindowShouldClose(m_impl->window) != 0;
}

VkInstance VulkanContext::instance() const noexcept
{
    return m_impl->instance;
}

VkPhysicalDevice VulkanContext::physical_device() const noexcept
{
    return m_impl->physical_device;
}

VkDevice VulkanContext::device() const noexcept
{
    return m_impl->device;
}

VkQueue VulkanContext::graphics_queue() const noexcept
{
    return m_impl->graphics_queue;
}

VkQueue VulkanContext::present_queue() const noexcept
{
    return m_impl->present_queue;
}

VkSurfaceKHR VulkanContext::surface() const noexcept
{
    return m_impl->surface;
}

} // namespace vulkano
