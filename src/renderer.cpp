#include <stdexcept>
#include <vector>

#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#include <vulkan_app/renderer.hpp>

namespace vulkan_app {

namespace {
#ifndef NDEBUG
constexpr bool kEnableValidation{true};
#else
constexpr bool kEnableValidation{false};
#endif

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* /*user_data*/) {
    (void)type;
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        fprintf(stderr, "Vulkan: %s\n", callback_data->pMessage);
    }
    return VK_FALSE;
}

} // namespace

Renderer::Renderer(GLFWwindow* window) {
    init_instance();
    init_surface(window);
    pick_physical_device();
    create_device();
    create_swapchain();
}

Renderer::~Renderer() noexcept {
    destroy_swapchain();
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        vkDestroyDevice(device_, nullptr);
    }
    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
    }
    if (debug_messenger_ != VK_NULL_HANDLE) {
        auto destroyFn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroyFn != nullptr) {
            destroyFn(instance_, debug_messenger_, nullptr);
        }
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
    }
}

void Renderer::init_instance() {
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "vulkan_app";
    app_info.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    app_info.pEngineName = "vulkan_app";
    app_info.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    std::uint32_t glfw_count{0U};
    const char** glfw_ext = glfwGetRequiredInstanceExtensions(&glfw_count);
    std::vector<const char*> extensions(glfw_ext, glfw_ext + glfw_count);
    if (kEnableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    const std::vector<const char*> layers{
        "VK_LAYER_KHRONOS_validation"
    };

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app_info;
    ci.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();
    if (kEnableValidation) {
        ci.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
        ci.ppEnabledLayerNames = layers.data();
    }

    VkDebugUtilsMessengerCreateInfoEXT debug_ci{};
    if (kEnableValidation) {
        debug_ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUGUTILS_MESSAGETYPE_PERFORMANCE_BIT_EXT;
        debug_ci.pfnUserCallback = debug_callback;
        ci.pNext = &debug_ci;
    }

    if (vkCreateInstance(&ci, nullptr, &instance_) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create Vulkan instance"};
    }

    if (kEnableValidation) {
        auto createFn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
        if (createFn != nullptr) {
            if (createFn(instance_, &debug_ci, nullptr, &debug_messenger_) != VK_SUCCESS) {
                fprintf(stderr, "Failed to set up debug messenger\n");
            }
        }
    }
}

void Renderer::init_surface(GLFWwindow* window) {
    if (glfwCreateWindowSurface(instance_, window, nullptr, &surface_) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create window surface"};
    }
}

void Renderer::pick_physical_device() {
    std::uint32_t count{0U};
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0U) {
        throw std::runtime_error{"No Vulkan-capable GPUs found"};
    }
    std::vector<VkPhysicalDevice> gpus(count);
    vkEnumeratePhysicalDevices(instance_, &count, gpus.data());
    physical_device_ = gpus[0];
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physical_device_, &props);
    device_name_ = props.deviceName;
}

void Renderer::create_device() {
    // Query queue families
    std::uint32_t family_count{0U};
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &family_count, families.data());

    // Naive selection: first with graphics; assume present is same.
    for (std::uint32_t i{0U}; i < family_count; ++i) {
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
            graphics_family_ = i;
            present_family_ = i;
            break;
        }
    }
    const float prio{1.0F};
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = graphics_family_;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;

    VkPhysicalDeviceFeatures features{};

    const char* device_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.pQueueCreateInfos = &qci;
    dci.queueCreateInfoCount = 1;
    dci.pEnabledFeatures = &features;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = device_exts;
    if (vkCreateDevice(physical_device_, &dci, nullptr, &device_) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create logical device"};
    }
    vkGetDeviceQueue(device_, graphics_family_, 0, &graphics_queue_);
    present_queue_ = graphics_queue_;
}

void Renderer::create_swapchain() {
    (void)swapchain_;
    (void)swapchain_extent_;
    (void)swapchain_format_;
    // Stub for now; full implementation will query surface caps and create swapchain + image views.
}

void Renderer::destroy_swapchain() {
    if (device_ != VK_NULL_HANDLE && swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

void Renderer::resize() {
    // Stub: will recreate swapchain and dependent resources.
}

void Renderer::update_mesh(const Mesh& /*mesh*/) {
    // Stub: upload vertex/index buffers.
}

void Renderer::draw_frame(const CameraUBO& /*camera*/, const Light& /*light*/, const Material& /*material*/, const SsaoParams& /*ssao*/) {
    // Stub: will record command buffers for passes and present.
}

} // namespace vulkan_app

