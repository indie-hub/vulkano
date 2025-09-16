#include <vulkano/vulkan_context.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

namespace vulkano {

namespace {
    constexpr const char* kValidationLayer {"VK_LAYER_KHRONOS_validation"};
    constexpr const char* kExtDebugUtils {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
    constexpr const char* kExtPortability {VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME};
    constexpr const char* kExtGetPhysProps2 {VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME};
    constexpr const char* kExtSwapchain {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    constexpr const char* kExtPortabilitySubset {"VK_KHR_portability_subset"};

    constexpr float kDefaultQueuePriority {1.0F};

    struct Vertex final {
        glm::vec2 pos {};
    };

    constexpr std::array<Vertex, 3> kTriangleVertices {
        Vertex {glm::vec2 {-0.5F, -0.5F}},
        Vertex {glm::vec2 {0.5F, -0.5F}},
        Vertex {glm::vec2 {0.0F, 0.5F}},
    };

    [[nodiscard]] std::vector<char> read_file_binary(const std::string& path) noexcept {
        std::vector<char> buffer {};
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file) {
            return buffer;
        }
        const std::streamsize size {file.tellg()};
        if (size <= 0) {
            return buffer;
        }
        buffer.resize(static_cast<std::size_t>(size));
        file.seekg(0, std::ios::beg);
        file.read(buffer.data(), size);
        return buffer;
    }

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
    create_swapchain_and_views(window);
    create_render_pass();
    create_pipeline_layout();
    create_graphics_pipeline();
    create_vertex_buffer();
    create_framebuffers();
    create_command_pool_and_buffers();
    create_sync_objects();
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

VkFormat VulkanContext::swapchain_image_format() const noexcept {
    return swapchain_image_format_;
}

VkExtent2D VulkanContext::swapchain_extent() const noexcept {
    return swapchain_extent_;
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
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }
    destroy_sync_objects();
    destroy_command_pool_and_buffers();
    destroy_framebuffers();
    destroy_render_pass();
    destroy_pipeline();
    destroy_vertex_buffer();
    destroy_swapchain_and_views();
    if (device_ != VK_NULL_HANDLE) {
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
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
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

namespace {
    struct SwapchainSupportDetails final {
        VkSurfaceCapabilitiesKHR capabilities {};
        std::vector<VkSurfaceFormatKHR> formats {};
        std::vector<VkPresentModeKHR> presentModes {};
    };

    [[nodiscard]] SwapchainSupportDetails query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface) noexcept {
        SwapchainSupportDetails details {};
        if (device == VK_NULL_HANDLE || surface == VK_NULL_HANDLE) {
            return details;
        }
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        std::uint32_t formatCount {0U};
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (formatCount != 0U) {
            details.formats.resize(static_cast<std::size_t>(formatCount));
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        std::uint32_t presentCount {0U};
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentCount, nullptr);
        if (presentCount != 0U) {
            details.presentModes.resize(static_cast<std::size_t>(presentCount));
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentCount, details.presentModes.data());
        }
        return details;
    }

    [[nodiscard]] VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& available) noexcept {
        // Prefer SRGB non-linear with RGBA/BGRA 8-bit
        for (const auto& fmt : available) {
            if ((fmt.format == VK_FORMAT_B8G8R8A8_SRGB || fmt.format == VK_FORMAT_R8G8B8A8_SRGB) &&
                fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return fmt;
            }
        }
        // Fallback to first available
        if (!available.empty()) {
            return available.front();
        }
        VkSurfaceFormatKHR unsupported {};
        unsupported.format = VK_FORMAT_B8G8R8A8_UNORM;
        unsupported.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        return unsupported;
    }

    [[nodiscard]] VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& available) noexcept {
        // Prefer MAILBOX; fallback to FIFO (guaranteed)
        for (const auto& mode : available) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return mode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    [[nodiscard]] VkExtent2D clamp_extent(const VkSurfaceCapabilitiesKHR& caps, VkExtent2D extent) noexcept {
        VkExtent2D e {};
        e.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
        e.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
        return e;
    }
}

void VulkanContext::create_swapchain_and_views(GLFWwindow* window) noexcept {
    if (physical_device_ == VK_NULL_HANDLE || device_ == VK_NULL_HANDLE || surface_ == VK_NULL_HANDLE) {
        return;
    }

    const SwapchainSupportDetails support {query_swapchain_support(physical_device_, surface_)};
    if (support.formats.empty() || support.presentModes.empty()) {
        return;
    }

    const VkSurfaceFormatKHR surfaceFormat {choose_surface_format(support.formats)};
    const VkPresentModeKHR presentMode {choose_present_mode(support.presentModes)};

    VkExtent2D extent {};
    if (support.capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        extent = support.capabilities.currentExtent;
    } else {
        int fbWidth {0};
        int fbHeight {0};
        if (window != nullptr) {
            glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        }
        VkExtent2D desired {static_cast<std::uint32_t>(fbWidth > 0 ? fbWidth : 1), static_cast<std::uint32_t>(fbHeight > 0 ? fbHeight : 1)};
        extent = clamp_extent(support.capabilities, desired);
    }

    std::uint32_t imageCount {support.capabilities.minImageCount + 1U};
    if (support.capabilities.maxImageCount > 0U && imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1U;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    std::array<std::uint32_t, 2> indices {graphics_family_index_, present_family_index_};
    if (graphics_family_index_ != present_family_index_) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2U;
        createInfo.pQueueFamilyIndices = indices.data();
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0U;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain {VK_NULL_HANDLE};
    const VkResult res {vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain)};
    if (res != VK_SUCCESS) {
        return;
    }
    swapchain_ = swapchain;
    swapchain_image_format_ = surfaceFormat.format;
    swapchain_extent_ = extent;

    std::uint32_t retrievedCount {0U};
    vkGetSwapchainImagesKHR(device_, swapchain_, &retrievedCount, nullptr);
    if (retrievedCount == 0U) {
        return;
    }
    swapchain_images_.resize(static_cast<std::size_t>(retrievedCount));
    vkGetSwapchainImagesKHR(device_, swapchain_, &retrievedCount, swapchain_images_.data());

    swapchain_image_views_.resize(swapchain_images_.size());
    images_in_flight_.assign(swapchain_images_.size(), VK_NULL_HANDLE);
    for (std::size_t i {0U}; i < swapchain_images_.size(); ++i) {
        VkImageViewCreateInfo viewInfo {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchain_images_[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchain_image_format_;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0U;
        viewInfo.subresourceRange.levelCount = 1U;
        viewInfo.subresourceRange.baseArrayLayer = 0U;
        viewInfo.subresourceRange.layerCount = 1U;
        VkImageView view {VK_NULL_HANDLE};
        if (vkCreateImageView(device_, &viewInfo, nullptr, &view) == VK_SUCCESS) {
            swapchain_image_views_[i] = view;
        } else {
            swapchain_image_views_[i] = VK_NULL_HANDLE;
        }
    }
}

void VulkanContext::destroy_swapchain_and_views() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    destroy_framebuffers();
    for (auto& view : swapchain_image_views_) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, view, nullptr);
            view = VK_NULL_HANDLE;
        }
    }
    swapchain_image_views_.clear();
    swapchain_images_.clear();
    images_in_flight_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::create_render_pass() noexcept {
    if (device_ == VK_NULL_HANDLE || swapchain_image_format_ == VK_FORMAT_UNDEFINED) {
        return;
    }

    VkAttachmentDescription colorAttachment {};
    colorAttachment.format = swapchain_image_format_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef {};
    colorAttachmentRef.attachment = 0U;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1U;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0U;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0U;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1U;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1U;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1U;
    renderPassInfo.pDependencies = &dependency;

    VkRenderPass rp {VK_NULL_HANDLE};
    if (vkCreateRenderPass(device_, &renderPassInfo, nullptr, &rp) == VK_SUCCESS) {
        render_pass_ = rp;
    }
}

void VulkanContext::create_framebuffers() noexcept {
    if (device_ == VK_NULL_HANDLE || render_pass_ == VK_NULL_HANDLE || swapchain_extent_.width == 0U || swapchain_image_views_.empty()) {
        return;
    }
    framebuffers_.resize(swapchain_image_views_.size());
    for (std::size_t i {0U}; i < swapchain_image_views_.size(); ++i) {
        VkImageView attachments[1] {swapchain_image_views_[i]};

        VkFramebufferCreateInfo fbInfo {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = render_pass_;
        fbInfo.attachmentCount = 1U;
        fbInfo.pAttachments = attachments;
        fbInfo.width = swapchain_extent_.width;
        fbInfo.height = swapchain_extent_.height;
        fbInfo.layers = 1U;
        VkFramebuffer fb {VK_NULL_HANDLE};
        if (vkCreateFramebuffer(device_, &fbInfo, nullptr, &fb) == VK_SUCCESS) {
            framebuffers_[i] = fb;
        } else {
            framebuffers_[i] = VK_NULL_HANDLE;
        }
    }
}

namespace {
    [[nodiscard]] bool file_exists(const std::string& path) noexcept {
        std::ifstream f(path.c_str(), std::ios::binary);
        return static_cast<bool>(f);
    }

    [[nodiscard]] std::string shader_dir_guess() noexcept {
        const char* env {std::getenv("VK_SHADER_DIR")};
        if (env != nullptr && std::strlen(env) > 0U) {
            const std::string dir {env};
            if (file_exists(dir + "/triangle.vert.spv") && file_exists(dir + "/triangle.frag.spv")) {
                return dir;
            }
        }
        // Prefer bin/shaders alongside binary when run from repo root
        if (file_exists("./bin/shaders/triangle.vert.spv") && file_exists("./bin/shaders/triangle.frag.spv")) {
            return std::string {"./bin/shaders"};
        }
        // Fallback to source shaders dir (in case of precompiled assets placed there)
        if (file_exists("./shaders/triangle.vert.spv") && file_exists("./shaders/triangle.frag.spv")) {
            return std::string {"./shaders"};
        }
        return std::string {"./shaders"};
    }

    [[nodiscard]] VkShaderModule create_shader_module(VkDevice device, const std::vector<char>& code) noexcept {
        if (device == VK_NULL_HANDLE || code.empty()) {
            return VK_NULL_HANDLE;
        }
        VkShaderModuleCreateInfo info {};
        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = code.size();
        info.pCode = reinterpret_cast<const std::uint32_t*>(code.data());
        VkShaderModule module {VK_NULL_HANDLE};
        if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS) {
            return VK_NULL_HANDLE;
        }
        return module;
    }
}

void VulkanContext::create_pipeline_layout() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    VkPushConstantRange range {};
    range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    range.offset = 0U;
    range.size = sizeof(float) * 4U;

    VkPipelineLayoutCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.setLayoutCount = 0U;
    info.pSetLayouts = nullptr;
    info.pushConstantRangeCount = 1U;
    info.pPushConstantRanges = &range;

    VkPipelineLayout layout {VK_NULL_HANDLE};
    if (vkCreatePipelineLayout(device_, &info, nullptr, &layout) == VK_SUCCESS) {
        pipeline_layout_ = layout;
    }
}

void VulkanContext::create_graphics_pipeline() noexcept {
    if (device_ == VK_NULL_HANDLE || render_pass_ == VK_NULL_HANDLE || pipeline_layout_ == VK_NULL_HANDLE) {
        return;
    }
    const std::string dir {shader_dir_guess()};
    const std::vector<char> vertCode {read_file_binary(dir + "/triangle.vert.spv")};
    const std::vector<char> fragCode {read_file_binary(dir + "/triangle.frag.spv")};
    if (vertCode.empty() || fragCode.empty()) {
        return;
    }
    const VkShaderModule vertModule {create_shader_module(device_, vertCode)};
    const VkShaderModule fragModule {create_shader_module(device_, fragCode)};
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
        if (vertModule != VK_NULL_HANDLE) { vkDestroyShaderModule(device_, vertModule, nullptr); }
        if (fragModule != VK_NULL_HANDLE) { vkDestroyShaderModule(device_, fragModule, nullptr); }
        return;
    }

    VkPipelineShaderStageCreateInfo vertStage {};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage {};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "main";

    const VkPipelineShaderStageCreateInfo stages[2] {vertStage, fragStage};

    VkVertexInputBindingDescription binding {};
    binding.binding = 0U;
    binding.stride = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrib {};
    attrib.location = 0U;
    attrib.binding = 0U;
    attrib.format = VK_FORMAT_R32G32_SFLOAT;
    attrib.offset = 0U;

    VkPipelineVertexInputStateCreateInfo vertexInput {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1U;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 1U;
    vertexInput.pVertexAttributeDescriptions = &attrib;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport {};
    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = static_cast<float>(swapchain_extent_.width);
    viewport.height = static_cast<float>(swapchain_extent_.height);
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    VkRect2D scissor {};
    scissor.offset = {0, 0};
    scissor.extent = swapchain_extent_;

    VkPipelineViewportStateCreateInfo viewportState {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1U;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1U;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo raster {};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.depthClampEnable = VK_FALSE;
    raster.rasterizerDiscardEnable = VK_FALSE;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_BACK_BIT;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.depthBiasEnable = VK_FALSE;
    raster.lineWidth = 1.0F;

    VkPipelineMultisampleStateCreateInfo msaa {};
    msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    msaa.sampleShadingEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttach {};
    colorBlendAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttach.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlend {};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.logicOpEnable = VK_FALSE;
    colorBlend.attachmentCount = 1U;
    colorBlend.pAttachments = &colorBlendAttach;

    VkGraphicsPipelineCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = 2U;
    info.pStages = stages;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &inputAssembly;
    info.pViewportState = &viewportState;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &msaa;
    info.pDepthStencilState = nullptr;
    info.pColorBlendState = &colorBlend;
    info.pDynamicState = nullptr;
    info.layout = pipeline_layout_;
    info.renderPass = render_pass_;
    info.subpass = 0U;

    VkPipeline pipeline {VK_NULL_HANDLE};
    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1U, &info, nullptr, &pipeline) == VK_SUCCESS) {
        graphics_pipeline_ = pipeline;
    }

    vkDestroyShaderModule(device_, vertModule, nullptr);
    vkDestroyShaderModule(device_, fragModule, nullptr);
}

namespace {
    [[nodiscard]] std::uint32_t find_memory_type(VkPhysicalDevice phys, std::uint32_t typeFilter, VkMemoryPropertyFlags props) noexcept {
        VkPhysicalDeviceMemoryProperties memProps {};
        vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
        for (std::uint32_t i {0U}; i < memProps.memoryTypeCount; ++i) {
            if ((typeFilter & (1U << i)) != 0U && (memProps.memoryTypes[i].propertyFlags & props) == props) {
                return i;
            }
        }
        return UINT32_MAX;
    }
}

void VulkanContext::create_vertex_buffer() noexcept {
    if (device_ == VK_NULL_HANDLE || physical_device_ == VK_NULL_HANDLE) {
        return;
    }
    const VkDeviceSize bufferSize {static_cast<VkDeviceSize>(kTriangleVertices.size() * sizeof(kTriangleVertices[0]))};

    VkBufferCreateInfo bufInfo {};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = bufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer buffer {VK_NULL_HANDLE};
    if (vkCreateBuffer(device_, &bufInfo, nullptr, &buffer) != VK_SUCCESS) {
        return;
    }

    VkMemoryRequirements memReq {};
    vkGetBufferMemoryRequirements(device_, buffer, &memReq);
    const std::uint32_t typeIndex {find_memory_type(physical_device_, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
    if (typeIndex == UINT32_MAX) {
        vkDestroyBuffer(device_, buffer, nullptr);
        return;
    }

    VkMemoryAllocateInfo allocInfo {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = typeIndex;
    VkDeviceMemory memory {VK_NULL_HANDLE};
    if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(device_, buffer, nullptr);
        return;
    }

    vkBindBufferMemory(device_, buffer, memory, 0U);

    void* data {nullptr};
    if (vkMapMemory(device_, memory, 0U, bufferSize, 0U, &data) == VK_SUCCESS) {
        std::memcpy(data, kTriangleVertices.data(), static_cast<std::size_t>(bufferSize));
        vkUnmapMemory(device_, memory);
    }

    vertex_buffer_ = buffer;
    vertex_buffer_memory_ = memory;
}

void VulkanContext::create_command_pool_and_buffers() noexcept {
    if (device_ == VK_NULL_HANDLE || graphics_family_index_ == UINT32_MAX || framebuffers_.empty()) {
        return;
    }

    if (command_pool_ == VK_NULL_HANDLE) {
        VkCommandPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphics_family_index_;
        VkCommandPool pool {VK_NULL_HANDLE};
        if (vkCreateCommandPool(device_, &poolInfo, nullptr, &pool) == VK_SUCCESS) {
            command_pool_ = pool;
        }
    }

    if (command_pool_ == VK_NULL_HANDLE) {
        return;
    }

    command_buffers_.resize(framebuffers_.size());
    VkCommandBufferAllocateInfo allocInfo {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = command_pool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<std::uint32_t>(command_buffers_.size());

    if (!command_buffers_.empty()) {
        if (vkAllocateCommandBuffers(device_, &allocInfo, command_buffers_.data()) != VK_SUCCESS) {
            command_buffers_.clear();
        }
    }
}

void VulkanContext::create_sync_objects() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    VkSemaphoreCreateInfo semInfo {};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled so first frame does not block

    for (std::uint32_t i {0U}; i < kMaxFramesInFlight; ++i) {
        vkCreateSemaphore(device_, &semInfo, nullptr, &image_available_semaphores_[i]);
        vkCreateFence(device_, &fenceInfo, nullptr, &in_flight_fences_[i]);
    }

    // One render-finished semaphore per swapchain image to avoid reuse hazards across presents
    render_finished_semaphores_.assign(swapchain_images_.size(), VK_NULL_HANDLE);
    for (std::size_t i {0U}; i < swapchain_images_.size(); ++i) {
        vkCreateSemaphore(device_, &semInfo, nullptr, &render_finished_semaphores_[i]);
    }
}

void VulkanContext::destroy_framebuffers() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    for (auto& fb : framebuffers_) {
        if (fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, fb, nullptr);
            fb = VK_NULL_HANDLE;
        }
    }
    framebuffers_.clear();
}

void VulkanContext::destroy_sync_objects() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    for (std::uint32_t i {0U}; i < kMaxFramesInFlight; ++i) {
        if (image_available_semaphores_[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, image_available_semaphores_[i], nullptr);
            image_available_semaphores_[i] = VK_NULL_HANDLE;
        }
        if (in_flight_fences_[i] != VK_NULL_HANDLE) {
            vkDestroyFence(device_, in_flight_fences_[i], nullptr);
            in_flight_fences_[i] = VK_NULL_HANDLE;
        }
    }
    for (auto& s : render_finished_semaphores_) {
        if (s != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, s, nullptr);
            s = VK_NULL_HANDLE;
        }
    }
    render_finished_semaphores_.clear();
}

void VulkanContext::destroy_command_pool_and_buffers() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (!command_buffers_.empty()) {
        vkFreeCommandBuffers(device_, command_pool_, static_cast<std::uint32_t>(command_buffers_.size()), command_buffers_.data());
        command_buffers_.clear();
    }
    if (command_pool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, command_pool_, nullptr);
        command_pool_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::destroy_render_pass() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, render_pass_, nullptr);
        render_pass_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::destroy_pipeline() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (graphics_pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, graphics_pipeline_, nullptr);
        graphics_pipeline_ = VK_NULL_HANDLE;
    }
    if (pipeline_layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
        pipeline_layout_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::destroy_vertex_buffer() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (vertex_buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, vertex_buffer_, nullptr);
        vertex_buffer_ = VK_NULL_HANDLE;
    }
    if (vertex_buffer_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, vertex_buffer_memory_, nullptr);
        vertex_buffer_memory_ = VK_NULL_HANDLE;
    }
}


void VulkanContext::record_commands(std::uint32_t imageIndex) noexcept {
    if (device_ == VK_NULL_HANDLE || graphics_pipeline_ == VK_NULL_HANDLE || pipeline_layout_ == VK_NULL_HANDLE) {
        return;
    }
    if (imageIndex >= command_buffers_.size() || imageIndex >= framebuffers_.size()) {
        return;
    }

    VkCommandBuffer cmd {command_buffers_[imageIndex]};
    if (cmd == VK_NULL_HANDLE) {
        return;
    }

    vkResetCommandBuffer(cmd, 0U);

    VkCommandBufferBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        return;
    }

    VkClearValue clear {};
    clear.color = VkClearColorValue {{0.0F, 0.0F, 0.0F, 1.0F}};

    VkRenderPassBeginInfo rpInfo {};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = render_pass_;
    rpInfo.framebuffer = framebuffers_[imageIndex];
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = swapchain_extent_;
    rpInfo.clearValueCount = 1U;
    rpInfo.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_);

    const VkBuffer buffers[1] {vertex_buffer_};
    const VkDeviceSize offsets[1] {0U};
    vkCmdBindVertexBuffers(cmd, 0U, 1U, buffers, offsets);

    const float white[4] {1.0F, 1.0F, 1.0F, 1.0F};
    vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0U, sizeof(white), white);

    vkCmdDraw(cmd, static_cast<std::uint32_t>(kTriangleVertices.size()), 1U, 0U, 0U);

    vkCmdEndRenderPass(cmd);
    (void)vkEndCommandBuffer(cmd);
}

bool VulkanContext::draw_frame() noexcept {
    if (device_ == VK_NULL_HANDLE || swapchain_ == VK_NULL_HANDLE) {
        return false;
    }

    const std::uint32_t frameIndex {current_frame_ % kMaxFramesInFlight};
    if (in_flight_fences_[frameIndex] != VK_NULL_HANDLE) {
        (void)vkWaitForFences(device_, 1U, &in_flight_fences_[frameIndex], VK_TRUE, UINT64_MAX);
        (void)vkResetFences(device_, 1U, &in_flight_fences_[frameIndex]);
    }

    std::uint32_t imageIndex {0U};
    const VkResult acquireRes {vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, image_available_semaphores_[frameIndex], VK_NULL_HANDLE, &imageIndex)};
    if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR) {
        return false; // Will be handled in resize step
    }
    if (acquireRes != VK_SUBOPTIMAL_KHR && acquireRes != VK_SUCCESS) {
        return false;
    }

    if (!images_in_flight_.empty() && images_in_flight_[imageIndex] != VK_NULL_HANDLE) {
        (void)vkWaitForFences(device_, 1U, &images_in_flight_[imageIndex], VK_TRUE, UINT64_MAX);
    }
    if (!images_in_flight_.empty()) {
        images_in_flight_[imageIndex] = in_flight_fences_[frameIndex];
    }

    record_commands(imageIndex);

    const VkPipelineStageFlags waitStages[1] {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1U;
    submitInfo.pWaitSemaphores = &image_available_semaphores_[frameIndex];
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1U;
    const VkCommandBuffer cmdBuf {command_buffers_[imageIndex]};
    submitInfo.pCommandBuffers = &cmdBuf;
    submitInfo.signalSemaphoreCount = 1U;
    VkSemaphore signalSem = render_finished_semaphores_.empty() ? VK_NULL_HANDLE : render_finished_semaphores_[imageIndex];
    submitInfo.pSignalSemaphores = &signalSem;

    if (vkQueueSubmit(graphics_queue_, 1U, &submitInfo, in_flight_fences_[frameIndex]) != VK_SUCCESS) {
        return false;
    }

    VkPresentInfoKHR presentInfo {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1U;
    presentInfo.pWaitSemaphores = &signalSem;
    presentInfo.swapchainCount = 1U;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    const VkResult presentRes {vkQueuePresentKHR(present_queue_, &presentInfo)};
    if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR) {
        // Skip frame; resize handling will recreate on next step
        current_frame_ = (current_frame_ + 1U) % kMaxFramesInFlight;
        return false;
    }

    current_frame_ = (current_frame_ + 1U) % kMaxFramesInFlight;
    return true;
}

} // namespace vulkano
