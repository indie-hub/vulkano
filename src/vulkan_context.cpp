#include <vulkano/vulkan_context.hpp>
#include <vulkano/window.hpp>

#if VULKANO_HAS_VULKAN
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <array>
#include <stdexcept>

namespace vulkano {

struct VulkanContext::Impl final {
    VkInstance instance {VK_NULL_HANDLE};
    VkSurfaceKHR surface {VK_NULL_HANDLE};
    VkPhysicalDevice physical_device {VK_NULL_HANDLE};
    VkDevice device {VK_NULL_HANDLE};
    VkQueue graphics_queue {VK_NULL_HANDLE};
    uint32_t graphics_queue_family {0U};

    Impl() = default;
    ~Impl() = default;
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
    // Simple stderr print would go here in future
    (void)pCallbackData;
    return VK_FALSE;
}

static void create_instance(VkInstance* instance) {
    VkApplicationInfo app_info {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "vulkano";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "vulkano";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    uint32_t ext_count = 0U;
    const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);
    std::array<const char*, 4> extensions {};
    for (uint32_t i = 0; i < ext_count; ++i) {
        extensions[i] = exts[i];
    }

    VkInstanceCreateInfo ci {};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app_info;
    ci.enabledExtensionCount = ext_count;
    ci.ppEnabledExtensionNames = extensions.data();

#ifndef NDEBUG
    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
    ci.enabledLayerCount = 1U;
    ci.ppEnabledLayerNames = layers;
#endif

    const VkResult res = vkCreateInstance(&ci, nullptr, instance);
    if (res != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create Vulkan instance"};
    }
}

VulkanContext::VulkanContext(const Window& window)
    : impl {std::make_unique<Impl>()} {
    create_instance(&impl->instance);
    GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window.native_handle());
    if (glfw_win == nullptr) {
        throw std::runtime_error {"Invalid GLFW window handle"};
    }
    if (glfwCreateWindowSurface(impl->instance, glfw_win, nullptr, &impl->surface) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create Vulkan surface"};
    }
    // Physical device selection (very naive)
    uint32_t count = 0U;
    vkEnumeratePhysicalDevices(impl->instance, &count, nullptr);
    if (count == 0U) {
        throw std::runtime_error {"No Vulkan physical devices found"};
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(impl->instance, &count, devices.data());
    impl->physical_device = devices[0];

    // Queue family selection
    uint32_t qcount = 0U;
    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, nullptr);
    std::vector<VkQueueFamilyProperties> qprops(qcount);
    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, qprops.data());
    uint32_t gfx_index = UINT32_MAX;
    for (uint32_t i = 0; i < qcount; ++i) {
        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
            gfx_index = i;
            break;
        }
    }
    if (gfx_index == UINT32_MAX) {
        throw std::runtime_error {"No graphics queue family found"};
    }
    impl->graphics_queue_family = gfx_index;

    const float priority = 1.0F;
    VkDeviceQueueCreateInfo qci {};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = gfx_index;
    qci.queueCount = 1U;
    qci.pQueuePriorities = &priority;

    VkDeviceCreateInfo dci {};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1U;
    dci.pQueueCreateInfos = &qci;

    if (vkCreateDevice(impl->physical_device, &dci, nullptr, &impl->device) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create logical device"};
    }
    vkGetDeviceQueue(impl->device, gfx_index, 0U, &impl->graphics_queue);
}

VulkanContext::~VulkanContext() {
    if (impl != nullptr) {
        if (impl->device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(impl->device);
        }
        if (impl->device != VK_NULL_HANDLE) {
            vkDestroyDevice(impl->device, nullptr);
        }
        if (impl->surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(impl->instance, impl->surface, nullptr);
        }
        if (impl->instance != VK_NULL_HANDLE) {
            vkDestroyInstance(impl->instance, nullptr);
        }
    }
}

VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;

void VulkanContext::wait_idle() const noexcept {
    if (impl->device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(impl->device);
    }
}

void VulkanContext::draw_frame() {
    // Placeholder: swapchain/present will be added. No-op for now.
}

} // namespace vulkano

#else

namespace vulkano {

class Window;

VulkanContext::VulkanContext(const Window&) : impl {nullptr} {
}

VulkanContext::~VulkanContext() = default;
VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
void VulkanContext::wait_idle() const noexcept {
}
void VulkanContext::draw_frame() {
}

} // namespace vulkano

#endif

