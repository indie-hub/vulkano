#include <vulkano/vulkan_context.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <vector>

namespace vulkano {

namespace {
    constexpr const char* kValidationLayer {"VK_LAYER_KHRONOS_validation"};
    constexpr const char* kExtDebugUtils {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
    constexpr const char* kExtPortability {VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME};
    constexpr const char* kExtGetPhysProps2 {VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME};
    constexpr const char* kExtSwapchain {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    constexpr const char* kExtPortabilitySubset {"VK_KHR_portability_subset"};

    constexpr float kDefaultQueuePriority {1.0F};

    [[nodiscard]] bool has_layer(const char* name) noexcept {
        std::uint32_t count {0U};
        if (vkEnumerateInstanceLayerProperties(&count, nullptr) != VK_SUCCESS) {
            return false;
        }
        std::vector<VkLayerProperties> props(static_cast<std::size_t>(count));
        if (vkEnumerateInstanceLayerProperties(&count, props.data()) != VK_SUCCESS) {
            return false;
        }
        for (const auto& p : props) {
            if (std::strcmp(p.layerName, name) == 0) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool has_extension(const char* name) noexcept {
        std::uint32_t count {0U};
        if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS) {
            return false;
        }
        std::vector<VkExtensionProperties> props(static_cast<std::size_t>(count));
        if (vkEnumerateInstanceExtensionProperties(nullptr, &count, props.data()) != VK_SUCCESS) {
            return false;
        }
        for (const auto& p : props) {
            if (std::strcmp(p.extensionName, name) == 0) {
                return true;
            }
        }
        return false;
    }

    using PFN_CreateDebugUtilsMessengerEXT = PFN_vkCreateDebugUtilsMessengerEXT;
    using PFN_DestroyDebugUtilsMessengerEXT = PFN_vkDestroyDebugUtilsMessengerEXT;

    [[nodiscard]] PFN_CreateDebugUtilsMessengerEXT load_create_debug(VkInstance instance) noexcept {
        return reinterpret_cast<PFN_CreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    }
    [[nodiscard]] PFN_DestroyDebugUtilsMessengerEXT load_destroy_debug(VkInstance instance) noexcept {
        return reinterpret_cast<PFN_DestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    }
}

VulkanContext::VulkanContext(GLFWwindow* window) noexcept {
    create_instance(window);
    setup_debug_utils();
    create_surface(window);
    pick_physical_device();
    create_logical_device();
}

VulkanContext::~VulkanContext() noexcept {
    destroy();
}

VkInstance VulkanContext::instance() const noexcept {
    return instance_;
}

VkSurfaceKHR VulkanContext::surface() const noexcept {
    return surface_;
}

bool VulkanContext::validation_enabled() const noexcept {
    return validation_enabled_;
}

VkPhysicalDevice VulkanContext::physical_device() const noexcept {
    return physical_device_;
}

VkDevice VulkanContext::device() const noexcept {
    return device_;
}

VkQueue VulkanContext::graphics_queue() const noexcept {
    return graphics_queue_;
}

VkQueue VulkanContext::present_queue() const noexcept {
    return present_queue_;
}

std::uint32_t VulkanContext::graphics_queue_family() const noexcept {
    return graphics_family_index_;
}

std::uint32_t VulkanContext::present_queue_family() const noexcept {
    return present_family_index_;
}

void VulkanContext::create_instance(GLFWwindow* window) noexcept {
    if (window == nullptr) {
        return;
    }

    if (glfwVulkanSupported() != GLFW_TRUE) {
        return;
    }

    std::uint32_t glfwExtCount {0U};
    const char** glfwExts {glfwGetRequiredInstanceExtensions(&glfwExtCount)};
    if (glfwExts == nullptr || glfwExtCount == 0U) {
        return;
    }

    std::vector<const char*> extensions {};
    extensions.reserve(static_cast<std::size_t>(glfwExtCount + 3U));
    for (std::uint32_t i {0U}; i < glfwExtCount; ++i) {
        extensions.push_back(glfwExts[i]);
    }

#ifndef NDEBUG
    validation_enabled_ = has_layer(kValidationLayer) && has_extension(kExtDebugUtils);
    if (validation_enabled_) {
        extensions.push_back(kExtDebugUtils);
    }
#else
    validation_enabled_ = false;
#endif

    if (has_extension(kExtPortability)) {
        extensions.push_back(kExtPortability);
    }
    if (has_extension(kExtGetPhysProps2)) {
        extensions.push_back(kExtGetPhysProps2);
    }

    VkApplicationInfo appInfo {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "vulkano_codex";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    appInfo.pEngineName = "vulkano";
    appInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3; // 1.3 as baseline; portability may expose 1.3

    VkInstanceCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT dbgInfo {};
    if (validation_enabled_) {
        const std::array<const char*, 1> layers {kValidationLayer};
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
        createInfo.ppEnabledLayerNames = layers.data();

        dbgInfo = {};
        dbgInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        dbgInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbgInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbgInfo.pfnUserCallback = &VulkanContext::debug_callback;
        dbgInfo.pUserData = nullptr;
        createInfo.pNext = &dbgInfo;
    } else {
        createInfo.enabledLayerCount = 0U;
        createInfo.ppEnabledLayerNames = nullptr;
        createInfo.pNext = nullptr;
    }

    // Portability flag if needed (mostly macOS/MoltenVK)
    if (has_extension(kExtPortability)) {
        createInfo.flags |= static_cast<VkInstanceCreateFlags>(VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR);
    }

    const VkResult res {vkCreateInstance(&createInfo, nullptr, &instance_)};
    if (res != VK_SUCCESS) {
        instance_ = VK_NULL_HANDLE;
        validation_enabled_ = false;
    }
}

void VulkanContext::setup_debug_utils() noexcept {
    if (!validation_enabled_ || instance_ == VK_NULL_HANDLE) {
        return;
    }
    VkDebugUtilsMessengerCreateInfoEXT info {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = &VulkanContext::debug_callback;
    info.pUserData = nullptr;

    const auto pfnCreate {load_create_debug(instance_)};
    if (pfnCreate == nullptr) {
        return;
    }
    const VkResult res {pfnCreate(instance_, &info, nullptr, &debug_messenger_)};
    if (res != VK_SUCCESS) {
        debug_messenger_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::create_surface(GLFWwindow* window) noexcept {
    if (instance_ == VK_NULL_HANDLE || window == nullptr) {
        return;
    }
    VkSurfaceKHR surface {VK_NULL_HANDLE};
    const VkResult res {glfwCreateWindowSurface(instance_, window, nullptr, &surface)};
    if (res == VK_SUCCESS) {
        surface_ = surface;
    }
}

void VulkanContext::pick_physical_device() noexcept {
    if (instance_ == VK_NULL_HANDLE) {
        return;
    }
    std::uint32_t count {0U};
    if (vkEnumeratePhysicalDevices(instance_, &count, nullptr) != VK_SUCCESS || count == 0U) {
        return;
    }
    std::vector<VkPhysicalDevice> devices(static_cast<std::size_t>(count));
    if (vkEnumeratePhysicalDevices(instance_, &count, devices.data()) != VK_SUCCESS) {
        return;
    }

    auto device_score = [this](VkPhysicalDevice dev) noexcept -> int {
        VkPhysicalDeviceProperties props {};
        vkGetPhysicalDeviceProperties(dev, &props);

        // Require queue families with graphics and present support
        std::uint32_t qCount {0U};
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qprops(static_cast<std::size_t>(qCount));
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, qprops.data());

        bool hasGraphics {false};
        bool hasPresent {false};
        for (std::uint32_t i {0U}; i < qCount; ++i) {
            if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT) {
                hasGraphics = true;
            }
            VkBool32 present {VK_FALSE};
            if (surface_ != VK_NULL_HANDLE) {
                vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface_, &present);
            }
            if (present == VK_TRUE) {
                hasPresent = true;
            }
        }
        if (!hasGraphics || !hasPresent) {
            return -1; // unsuitable
        }

        int score {0};
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            score += 500;
        }
        // Prefer higher maxImageDimension2D as a rough capability proxy
        score += static_cast<int>(props.limits.maxImageDimension2D);
        return score;
    };

    int bestScore {-1};
    VkPhysicalDevice best {VK_NULL_HANDLE};
    for (const auto dev : devices) {
        const int score {device_score(dev)};
        if (score > bestScore) {
            bestScore = score;
            best = dev;
        }
    }
    if (best == VK_NULL_HANDLE) {
        return;
    }

    // Record queue family indices
    std::uint32_t qCount {0U};
    vkGetPhysicalDeviceQueueFamilyProperties(best, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qprops(static_cast<std::size_t>(qCount));
    vkGetPhysicalDeviceQueueFamilyProperties(best, &qCount, qprops.data());
    for (std::uint32_t i {0U}; i < qCount; ++i) {
        if (graphics_family_index_ == UINT32_MAX && (qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT) {
            graphics_family_index_ = i;
        }
        VkBool32 present {VK_FALSE};
        if (surface_ != VK_NULL_HANDLE) {
            vkGetPhysicalDeviceSurfaceSupportKHR(best, i, surface_, &present);
        }
        if (present == VK_TRUE && present_family_index_ == UINT32_MAX) {
            present_family_index_ = i;
        }
    }

    if (graphics_family_index_ == UINT32_MAX || present_family_index_ == UINT32_MAX) {
        graphics_family_index_ = UINT32_MAX;
        present_family_index_ = UINT32_MAX;
        return;
    }

    physical_device_ = best;
}

void VulkanContext::create_logical_device() noexcept {
    if (physical_device_ == VK_NULL_HANDLE) {
        return;
    }

    // Unique queue family indices
    std::array<std::uint32_t, 2> families {graphics_family_index_, present_family_index_};
    if (graphics_family_index_ == present_family_index_) {
        families = {graphics_family_index_, UINT32_MAX};
    }

    std::vector<VkDeviceQueueCreateInfo> queueInfos {};
    queueInfos.reserve(2U);
    std::vector<float> priorities {kDefaultQueuePriority};

    VkDeviceQueueCreateInfo qinfoG {};
    qinfoG.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qinfoG.queueFamilyIndex = graphics_family_index_;
    qinfoG.queueCount = 1U;
    qinfoG.pQueuePriorities = priorities.data();
    queueInfos.push_back(qinfoG);

    if (families[1] != UINT32_MAX) {
        VkDeviceQueueCreateInfo qinfoP {};
        qinfoP.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qinfoP.queueFamilyIndex = present_family_index_;
        qinfoP.queueCount = 1U;
        qinfoP.pQueuePriorities = priorities.data();
        queueInfos.push_back(qinfoP);
    }

    VkPhysicalDeviceFeatures features {};

    std::vector<const char*> deviceExts {kExtSwapchain};
    // On Apple/MoltenVK, portability subset is required
    std::uint32_t extCount {0U};
    vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> available(static_cast<std::size_t>(extCount));
    vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &extCount, available.data());
    const auto hasPortability = std::any_of(available.begin(), available.end(), [](const VkExtensionProperties& p) {
        return std::strcmp(p.extensionName, kExtPortabilitySubset) == 0;
    });
    if (hasPortability) {
        deviceExts.push_back(kExtPortabilitySubset);
    }

    VkDeviceCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueInfos.size());
    createInfo.pQueueCreateInfos = queueInfos.data();
    createInfo.pEnabledFeatures = &features;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExts.size());
    createInfo.ppEnabledExtensionNames = deviceExts.data();

    if (validation_enabled_) {
        const std::array<const char*, 1> layers {kValidationLayer};
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
        createInfo.ppEnabledLayerNames = layers.data();
    } else {
        createInfo.enabledLayerCount = 0U;
        createInfo.ppEnabledLayerNames = nullptr;
    }

    const VkResult res {vkCreateDevice(physical_device_, &createInfo, nullptr, &device_)};
    if (res != VK_SUCCESS) {
        device_ = VK_NULL_HANDLE;
        return;
    }
    vkGetDeviceQueue(device_, graphics_family_index_, 0U, &graphics_queue_);
    vkGetDeviceQueue(device_, present_family_index_, 0U, &present_queue_);
}

void VulkanContext::destroy() noexcept {
    if (instance_ == VK_NULL_HANDLE) {
        return;
    }
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
        graphics_queue_ = VK_NULL_HANDLE;
        present_queue_ = VK_NULL_HANDLE;
        graphics_family_index_ = UINT32_MAX;
        present_family_index_ = UINT32_MAX;
        physical_device_ = VK_NULL_HANDLE;
    }
    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    if (debug_messenger_ != VK_NULL_HANDLE) {
        const auto pfnDestroy {load_destroy_debug(instance_)};
        if (pfnDestroy != nullptr) {
            pfnDestroy(instance_, debug_messenger_, nullptr);
        }
        debug_messenger_ = VK_NULL_HANDLE;
    }
    vkDestroyInstance(instance_, nullptr);
    instance_ = VK_NULL_HANDLE;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) noexcept {
    (void)messageSeverity;
    (void)messageTypes;
    (void)pUserData;
    // Print minimal validation issues to stderr to avoid noisy logs
    if (pCallbackData != nullptr && pCallbackData->pMessage != nullptr) {
        fputs(pCallbackData->pMessage, stderr);
        fputc('\n', stderr);
    }
    return VK_FALSE;
}

} // namespace vulkano
