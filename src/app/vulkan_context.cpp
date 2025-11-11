#include <vulkano/app/vulkan_context.hpp>

#include <vulkano/app/window.hpp>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
#ifndef NDEBUG
constexpr bool enableValidationLayers {true};
#else
constexpr bool enableValidationLayers {false};
#endif

constexpr std::array<const char*, 1> validationLayers {"VK_LAYER_KHRONOS_validation"};

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT /*messageTypes*/, const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* /*userData*/) {
    const std::string_view message {callbackData != nullptr && callbackData->pMessage != nullptr
            ? callbackData->pMessage
            : "<no message>"};
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "[VULKAN] " << message << '\n';
    }
    return VK_FALSE;
}

void populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = VkDebugUtilsMessengerCreateInfoEXT {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debug_callback;
    createInfo.pUserData = nullptr;
}

VkResult create_debug_utils_messenger(VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
    VkDebugUtilsMessengerEXT* messenger) {
    const auto createFunction = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (createFunction == nullptr) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    return createFunction(instance, createInfo, nullptr, messenger);
}

void destroy_debug_utils_messenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger) {
    const auto destroyFunction = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (destroyFunction != nullptr) {
        destroyFunction(instance, messenger, nullptr);
    }
}

bool check_validation_layer_support() {
    std::uint32_t layerCount {0U};
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        const bool found = std::any_of(availableLayers.begin(), availableLayers.end(),
            [layerName](const VkLayerProperties& layerProperty) {
                return std::strcmp(layerName, layerProperty.layerName) == 0;
            });
        if (!found) {
            return false;
        }
    }
    return true;
}

std::vector<const char*> gather_required_instance_extensions() {
    std::uint32_t glfwExtensionCount {0U};
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (glfwExtensions == nullptr || glfwExtensionCount == 0U) {
        throw std::runtime_error {"Failed to query required GLFW Vulkan instance extensions"};
    }

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
#ifdef __APPLE__
    extensions.push_back("VK_KHR_portability_enumeration");
    extensions.push_back("VK_KHR_get_physical_device_properties2");
#endif
    return extensions;
}

VkInstance create_instance() {
    if (enableValidationLayers && !check_validation_layer_support()) {
        throw std::runtime_error {"Validation layers requested, but not available"};
    }

    VkApplicationInfo appInfo {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkano Renderer";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "Vulkano";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    auto extensions = gather_required_instance_extensions();

    VkInstanceCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo {};
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        populate_debug_messenger_create_info(debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0U;
        createInfo.pNext = nullptr;
    }

#ifdef __APPLE__
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    VkInstance instance {VK_NULL_HANDLE};
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create Vulkan instance"};
    }
    return instance;
}

VkDebugUtilsMessengerEXT setup_debug_messenger(VkInstance instance) {
    if (!enableValidationLayers) {
        return VK_NULL_HANDLE;
    }
    VkDebugUtilsMessengerCreateInfoEXT createInfo {};
    populate_debug_messenger_create_info(createInfo);
    VkDebugUtilsMessengerEXT messenger {VK_NULL_HANDLE};
    if (create_debug_utils_messenger(instance, &createInfo, &messenger) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to set up debug messenger"};
    }
    return messenger;
}

VkSurfaceKHR create_surface(VkInstance instance, const vulkano::app::Window& window) {
    VkSurfaceKHR surface {VK_NULL_HANDLE};
    if (glfwCreateWindowSurface(instance, window.handle(), nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create window surface"};
    }
    return surface;
}

struct QueueFamilyIndices {
    std::optional<std::uint32_t> graphicsFamily {};
    std::optional<std::uint32_t> presentFamily {};

    [[nodiscard]] bool is_complete() const noexcept {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

QueueFamilyIndices find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices {};

    std::uint32_t queueFamilyCount {0U};
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (std::uint32_t i {0U}; i < queueFamilyCount; ++i) {
        const VkQueueFamilyProperties& queueFamily = queueFamilies[i];
        if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
            indices.graphicsFamily = i;
        }
        VkBool32 presentSupport {VK_FALSE};
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport == VK_TRUE) {
            indices.presentFamily = i;
        }
        if (indices.is_complete()) {
            break;
        }
    }

    return indices;
}

std::vector<const char*> required_device_extensions() {
    std::vector<const char*> extensions {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#ifdef __APPLE__
    extensions.push_back("VK_KHR_portability_subset");
#endif
    return extensions;
}

bool check_device_extension_support(VkPhysicalDevice device) {
    std::uint32_t extensionCount {0U};
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    const auto requiredExtensions = required_device_extensions();
    for (const char* required : requiredExtensions) {
        const bool found = std::any_of(availableExtensions.begin(), availableExtensions.end(),
            [required](const VkExtensionProperties& property) {
                return std::strcmp(required, property.extensionName) == 0;
            });
        if (!found) {
            return false;
        }
    }
    return true;
}

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities {};
    std::vector<VkSurfaceFormatKHR> formats {};
    std::vector<VkPresentModeKHR> presentModes {};
};

SwapChainSupportDetails query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapChainSupportDetails details {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    std::uint32_t formatCount {0U};
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    details.formats.resize(formatCount);
    if (formatCount != 0U) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    std::uint32_t presentModeCount {0U};
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    details.presentModes.resize(presentModeCount);
    if (presentModeCount != 0U) {
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

bool is_device_suitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
    const QueueFamilyIndices indices = find_queue_families(device, surface);
    if (!indices.is_complete()) {
        return false;
    }

    if (!check_device_extension_support(device)) {
        return false;
    }

    const SwapChainSupportDetails swapChainSupport = query_swapchain_support(device, surface);
    if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty()) {
        return false;
    }

    VkPhysicalDeviceFeatures supportedFeatures {};
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);
    return supportedFeatures.samplerAnisotropy == VK_TRUE;
}

VkPhysicalDevice pick_physical_device(VkInstance instance, VkSurfaceKHR surface) {
    std::uint32_t deviceCount {0U};
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0U) {
        throw std::runtime_error {"Failed to find GPUs with Vulkan support"};
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (VkPhysicalDevice device : devices) {
        if (is_device_suitable(device, surface)) {
            return device;
        }
    }

    throw std::runtime_error {"Failed to find a suitable GPU"};
}

VkDevice create_logical_device(VkPhysicalDevice physicalDevice, const QueueFamilyIndices& indices) {
    std::set<std::uint32_t> uniqueQueueFamilies {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(uniqueQueueFamilies.size());
    const float queuePriority {1.0F};
    for (const std::uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1U;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;

    const auto extensions = required_device_extensions();
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0U;
    }

    VkDevice device {VK_NULL_HANDLE};
    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create logical device"};
    }

    return device;
}

VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const VkSurfaceFormatKHR& format : availableFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return availableFormats.front();
}

VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const VkPresentModeKHR presentMode : availablePresentModes) {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return presentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities, const vulkano::app::Window& window) {
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    VkExtent2D actualExtent = window.framebuffer_extent();
    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return actualExtent;
}

struct SwapchainCreationOutput {
    VkSwapchainKHR swapchain {VK_NULL_HANDLE};
    VkFormat format {VK_FORMAT_UNDEFINED};
    VkExtent2D extent {0U, 0U};
    std::vector<VkImage> images {};
};

SwapchainCreationOutput create_swapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
    const vulkano::app::Window& window, const QueueFamilyIndices& indices) {
    const SwapChainSupportDetails swapChainSupport = query_swapchain_support(physicalDevice, surface);
    if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty()) {
        throw std::runtime_error {"Swap chain support incomplete"};
    }

    const VkSurfaceFormatKHR surfaceFormat = choose_swap_surface_format(swapChainSupport.formats);
    const VkPresentModeKHR presentMode = choose_swap_present_mode(swapChainSupport.presentModes);
    const VkExtent2D extent = choose_swap_extent(swapChainSupport.capabilities, window);

    std::uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1U;
    if (swapChainSupport.capabilities.maxImageCount > 0U
        && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1U;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const std::uint32_t graphicsFamily = indices.graphicsFamily.value();
    const std::uint32_t presentFamily = indices.presentFamily.value();
    if (graphicsFamily != presentFamily) {
        const std::array<std::uint32_t, 2> queueFamilyIndices {graphicsFamily, presentFamily};
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = static_cast<std::uint32_t>(queueFamilyIndices.size());
        createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0U;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    SwapchainCreationOutput output {};
    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &output.swapchain) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create swap chain"};
    }

    vkGetSwapchainImagesKHR(device, output.swapchain, &imageCount, nullptr);
    output.images.resize(imageCount);
    vkGetSwapchainImagesKHR(device, output.swapchain, &imageCount, output.images.data());

    output.format = surfaceFormat.format;
    output.extent = extent;
    return output;
}

std::vector<VkImageView> create_image_views(VkDevice device, VkFormat format, const std::vector<VkImage>& images) {
    std::vector<VkImageView> imageViews;
    imageViews.reserve(images.size());

    for (VkImage image : images) {
        VkImageViewCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = format;
        createInfo.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY};
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0U;
        createInfo.subresourceRange.levelCount = 1U;
        createInfo.subresourceRange.baseArrayLayer = 0U;
        createInfo.subresourceRange.layerCount = 1U;

        VkImageView imageView {VK_NULL_HANDLE};
        if (vkCreateImageView(device, &createInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create image view"};
        }
        imageViews.push_back(imageView);
    }

    return imageViews;
}

void log_vulkan_error(const std::string& context, VkResult result) noexcept {
    if (result != VK_SUCCESS) {
        std::cerr << context << " failed with error code " << result << '\n';
    }
}
} // namespace

namespace vulkano::app {
VulkanContext::VulkanContext(const Window& window) {
    m_instance = create_instance();
    m_debugMessenger = setup_debug_messenger(m_instance);
    m_surface = create_surface(m_instance, window);
    m_physicalDevice = pick_physical_device(m_instance, m_surface);

    const QueueFamilyIndices indices = find_queue_families(m_physicalDevice, m_surface);
    m_graphicsQueueFamilyIndex = indices.graphicsFamily.value();
    m_presentQueueFamilyIndex = indices.presentFamily.value();
    m_device = create_logical_device(m_physicalDevice, indices);

    vkGetDeviceQueue(m_device, m_graphicsQueueFamilyIndex, 0U, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentQueueFamilyIndex, 0U, &m_presentQueue);

    const SwapchainCreationOutput swapchainOutput = create_swapchain(m_physicalDevice, m_device, m_surface, window, indices);
    m_swapchain = swapchainOutput.swapchain;
    m_swapchainFormat = swapchainOutput.format;
    m_swapchainExtent = swapchainOutput.extent;
    m_swapchainImages = swapchainOutput.images;
    m_swapchainImageViews = create_image_views(m_device, m_swapchainFormat, m_swapchainImages);
}

VulkanContext::~VulkanContext() noexcept {
    if (m_device != VK_NULL_HANDLE) {
        const VkResult waitResult = vkDeviceWaitIdle(m_device);
        log_vulkan_error("vkDeviceWaitIdle", waitResult);
        for (VkImageView imageView : m_swapchainImageViews) {
            if (imageView != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device, imageView, nullptr);
            }
        }
        m_swapchainImageViews.clear();

        if (m_swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
            m_swapchain = VK_NULL_HANDLE;
        }

        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    if (m_surface != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }

    if (m_debugMessenger != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE) {
        destroy_debug_utils_messenger(m_instance, m_debugMessenger);
        m_debugMessenger = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

void VulkanContext::wait_idle() const noexcept {
    if (m_device != VK_NULL_HANDLE) {
        const VkResult result = vkDeviceWaitIdle(m_device);
        log_vulkan_error("vkDeviceWaitIdle", result);
    }
}

VkInstance VulkanContext::instance() const noexcept {
    return m_instance;
}

VkPhysicalDevice VulkanContext::physical_device() const noexcept {
    return m_physicalDevice;
}

VkDevice VulkanContext::device() const noexcept {
    return m_device;
}

VkSurfaceKHR VulkanContext::surface() const noexcept {
    return m_surface;
}

VkQueue VulkanContext::graphics_queue() const noexcept {
    return m_graphicsQueue;
}

VkQueue VulkanContext::present_queue() const noexcept {
    return m_presentQueue;
}

std::uint32_t VulkanContext::graphics_queue_family_index() const noexcept {
    return m_graphicsQueueFamilyIndex;
}

std::uint32_t VulkanContext::present_queue_family_index() const noexcept {
    return m_presentQueueFamilyIndex;
}

VkSwapchainKHR VulkanContext::swapchain() const noexcept {
    return m_swapchain;
}

VkFormat VulkanContext::swapchain_image_format() const noexcept {
    return m_swapchainFormat;
}

VkExtent2D VulkanContext::swapchain_extent() const noexcept {
    return m_swapchainExtent;
}

const std::vector<VkImageView>& VulkanContext::swapchain_image_views() const noexcept {
    return m_swapchainImageViews;
}
} // namespace vulkano::app
