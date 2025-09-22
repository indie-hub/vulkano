#include <vulkano/vulkan_context.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

#include <GLFW/glfw3.h>

namespace {
    VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
        void*) {
        (void)type;
        if(callbackData == nullptr) {
            return VK_FALSE;
        }
        std::cerr << "Validation [" << severity << "]: " << callbackData->pMessage << '\n';
        return VK_FALSE;
    }

    void populate_debug_create_info(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = &debug_callback;
    }

    auto load_create_debug_messenger(VkInstance instance) -> PFN_vkCreateDebugUtilsMessengerEXT {
        const auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        return func;
    }

    auto load_destroy_debug_messenger(VkInstance instance) -> PFN_vkDestroyDebugUtilsMessengerEXT {
        const auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        return func;
    }

    constexpr std::array<const char*, 1U> validationLayers {"VK_LAYER_KHRONOS_validation"};
    constexpr std::array<const char*, 1U> deviceExtensions {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    constexpr std::uint32_t desiredApiVersion {VK_API_VERSION_1_3};
} // namespace

namespace vulkano {

VulkanContext::VulkanContext() = default;

VulkanContext::VulkanContext(const AppConfig& config, const Window& window)
    : m_config {config}
    , m_window {&window} {
    create_instance();
    setup_debug_messenger();
    create_surface();
    pick_physical_device();
    create_logical_device();
}

VulkanContext::VulkanContext(VulkanContext&& other) noexcept {
    move_from(std::move(other));
}

auto VulkanContext::operator=(VulkanContext&& other) noexcept -> VulkanContext& {
    if(this != &other) {
        cleanup();
        move_from(std::move(other));
    }
    return *this;
}

VulkanContext::~VulkanContext() noexcept {
    cleanup();
}

VulkanContextBuilder::VulkanContextBuilder()
    : m_config {nullptr}
    , m_window {nullptr} {
}

auto VulkanContextBuilder::with_config(const AppConfig& config) -> VulkanContextBuilder& {
    m_config = &config;
    return *this;
}

auto VulkanContextBuilder::with_window(const Window& window) -> VulkanContextBuilder& {
    m_window = &window;
    return *this;
}

auto VulkanContextBuilder::build() const -> VulkanContext {
    if(m_config == nullptr) {
        throw std::runtime_error {"AppConfig must be provided before building VulkanContext"};
    }
    if(m_window == nullptr) {
        throw std::runtime_error {"Window must be provided before building VulkanContext"};
    }
    return VulkanContext {*m_config, *m_window};
}

auto VulkanContext::instance() const noexcept -> VkInstance {
    return m_instance;
}

auto VulkanContext::surface() const noexcept -> VkSurfaceKHR {
    return m_surface;
}

auto VulkanContext::physical_device() const noexcept -> VkPhysicalDevice {
    return m_physicalDevice;
}

auto VulkanContext::device() const noexcept -> VkDevice {
    return m_device;
}

auto VulkanContext::graphics_queue() const noexcept -> VkQueue {
    return m_graphicsQueue;
}

auto VulkanContext::present_queue() const noexcept -> VkQueue {
    return m_presentQueue;
}

auto VulkanContext::graphics_queue_index() const noexcept -> std::uint32_t {
    return m_graphicsQueueIndex;
}

auto VulkanContext::present_queue_index() const noexcept -> std::uint32_t {
    return m_presentQueueIndex;
}

auto VulkanContext::device_properties() const noexcept -> VkPhysicalDeviceProperties {
    return m_deviceProperties;
}

auto VulkanContext::validation_enabled() const noexcept -> bool {
    return m_config.validation_enabled();
}

void VulkanContext::set_object_name(VkObjectType type, std::uint64_t handle, const std::string& name) const {
    if(m_setObjectName == nullptr || m_device == VK_NULL_HANDLE) {
        return;
    }
    VkDebugUtilsObjectNameInfoEXT info {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType = type;
    info.objectHandle = handle;
    info.pObjectName = name.c_str();
    m_setObjectName(m_device, &info);
}

void VulkanContext::begin_debug_label(VkCommandBuffer commandBuffer, const std::string& label) const {
    if(m_cmdBeginLabel == nullptr) {
        return;
    }
    VkDebugUtilsLabelEXT labelInfo {};
    labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    labelInfo.pLabelName = label.c_str();
    labelInfo.color[0] = 1.0F;
    labelInfo.color[1] = 1.0F;
    labelInfo.color[2] = 1.0F;
    labelInfo.color[3] = 1.0F;
    m_cmdBeginLabel(commandBuffer, &labelInfo);
}

void VulkanContext::end_debug_label(VkCommandBuffer commandBuffer) const {
    if(m_cmdEndLabel == nullptr) {
        return;
    }
    m_cmdEndLabel(commandBuffer);
}

void VulkanContext::create_instance() {
    if(m_config.validation_enabled() && !check_validation_layer_support()) {
        throw std::runtime_error {"Requested validation layers are unavailable"};
    }

    VkApplicationInfo appInfo {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = m_config.application_name().c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = m_config.engine_name().c_str();
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = desiredApiVersion;

    VkInstanceCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    const std::vector<const char*> extensions = required_instance_extensions();
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
#if defined(VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR)
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo {};
    if(m_config.validation_enabled()) {
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        populate_debug_create_info(debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0U;
        createInfo.ppEnabledLayerNames = nullptr;
        createInfo.pNext = nullptr;
    }

    const VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if(result != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create Vulkan instance"};
    }
}

void VulkanContext::setup_debug_messenger() {
    if(!m_config.validation_enabled()) {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo {};
    populate_debug_create_info(createInfo);

    const auto createFunc = load_create_debug_messenger(m_instance);
    if(createFunc == nullptr) {
        throw std::runtime_error {"Failed to load vkCreateDebugUtilsMessengerEXT"};
    }
    const VkResult result = createFunc(m_instance, &createInfo, nullptr, &m_debugMessenger);
    if(result != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create debug messenger"};
    }
    load_debug_functions();
}

void VulkanContext::create_surface() {
    if(m_window == nullptr) {
        throw std::runtime_error {"Window must be valid before creating surface"};
    }
    m_surface = m_window->create_surface(m_instance);
}

void VulkanContext::pick_physical_device() {
    std::uint32_t deviceCount {0U};
    VkResult result = vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if(result != VK_SUCCESS || deviceCount == 0U) {
        throw std::runtime_error {"No Vulkan-capable physical devices found"};
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    result = vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
    if(result != VK_SUCCESS) {
        throw std::runtime_error {"Failed to enumerate physical devices"};
    }

    for(const VkPhysicalDevice device : devices) {
        const QueueSelection selection = gather_queue_selection(device);
        if(!selection.graphicsIndex.has_value() || !selection.presentIndex.has_value()) {
            continue;
        }
        if(!check_device_extension_support(device)) {
            continue;
        }
        m_physicalDevice = device;
        m_graphicsQueueIndex = selection.graphicsIndex.value();
        m_presentQueueIndex = selection.presentIndex.value();
        vkGetPhysicalDeviceProperties(device, &m_deviceProperties);
        return;
    }
    throw std::runtime_error {"Failed to find a suitable physical device"};
}

void VulkanContext::create_logical_device() {
    std::set<std::uint32_t> uniqueQueues {m_graphicsQueueIndex, m_presentQueueIndex};
    std::vector<VkDeviceQueueCreateInfo> queueInfos {};
    const float queuePriority {1.0F};
    for(const std::uint32_t queueIndex : uniqueQueues) {
        VkDeviceQueueCreateInfo queueInfo {};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = queueIndex;
        queueInfo.queueCount = 1U;
        queueInfo.pQueuePriorities = &queuePriority;
        queueInfos.push_back(queueInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures {};

    VkDeviceCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueInfos.size());
    createInfo.pQueueCreateInfos = queueInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    createInfo.enabledLayerCount = 0U;
    createInfo.ppEnabledLayerNames = nullptr;

    const VkResult result = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
    if(result != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create logical device"};
    }

    vkGetDeviceQueue(m_device, m_graphicsQueueIndex, 0U, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentQueueIndex, 0U, &m_presentQueue);
}

auto VulkanContext::required_instance_extensions() const -> std::vector<const char*> {
    uint32_t glfwExtensionCount {0U};
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if(glfwExtensions == nullptr || glfwExtensionCount == 0U) {
        throw std::runtime_error {"Failed to acquire GLFW instance extensions"};
    }

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
#if defined(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)
    const auto portabilityFound = std::find_if(
        extensions.begin(),
        extensions.end(),
        [](const char* name) {
            return std::strcmp(name, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0;
        });
    if(portabilityFound == extensions.end()) {
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    }
#endif
    if(m_config.validation_enabled()) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

auto VulkanContext::check_validation_layer_support() const -> bool {
    std::uint32_t layerCount {0U};
    const VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    if(result != VK_SUCCESS) {
        return false;
    }
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for(const char* layerName : validationLayers) {
        const auto match = std::find_if(
            availableLayers.begin(),
            availableLayers.end(),
            [layerName](const VkLayerProperties& layer) {
                return std::strcmp(layer.layerName, layerName) == 0;
            });
        if(match == availableLayers.end()) {
            return false;
        }
    }
    return true;
}

auto VulkanContext::gather_queue_selection(VkPhysicalDevice candidate) const -> QueueSelection {
    QueueSelection selection {};

    std::uint32_t queueFamilyCount {0U};
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, queueFamilies.data());

    for(std::uint32_t index {0U}; index < queueFamilyCount; ++index) {
        const VkQueueFamilyProperties& properties = queueFamilies[index];
        if(properties.queueCount > 0U && (properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
            selection.graphicsIndex = index;
        }
        VkBool32 presentSupport {VK_FALSE};
        vkGetPhysicalDeviceSurfaceSupportKHR(candidate, index, m_surface, &presentSupport);
        if(presentSupport == VK_TRUE) {
            selection.presentIndex = index;
        }
        if(selection.graphicsIndex.has_value() && selection.presentIndex.has_value()) {
            break;
        }
    }
    return selection;
}

auto VulkanContext::check_device_extension_support(VkPhysicalDevice candidate) const -> bool {
    std::uint32_t extensionCount {0U};
    VkResult result = vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, nullptr);
    if(result != VK_SUCCESS) {
        return false;
    }

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    result = vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, availableExtensions.data());
    if(result != VK_SUCCESS) {
        return false;
    }

    for(const char* required : deviceExtensions) {
        const auto found = std::find_if(
            availableExtensions.begin(),
            availableExtensions.end(),
            [required](const VkExtensionProperties& extension) {
                return std::strcmp(extension.extensionName, required) == 0;
            });
        if(found == availableExtensions.end()) {
            return false;
        }
    }
    return true;
}

void VulkanContext::cleanup() noexcept {
    if(m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    if(m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    if(m_debugMessenger != VK_NULL_HANDLE) {
        const auto destroyFunc = load_destroy_debug_messenger(m_instance);
        if(destroyFunc != nullptr) {
            destroyFunc(m_instance, m_debugMessenger, nullptr);
        }
        m_debugMessenger = VK_NULL_HANDLE;
    }
    if(m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
    m_physicalDevice = VK_NULL_HANDLE;
    m_graphicsQueue = VK_NULL_HANDLE;
    m_presentQueue = VK_NULL_HANDLE;
    m_graphicsQueueIndex = 0U;
    m_presentQueueIndex = 0U;
    m_window = nullptr;
    m_setObjectName = nullptr;
    m_cmdBeginLabel = nullptr;
    m_cmdEndLabel = nullptr;
}

void VulkanContext::move_from(VulkanContext&& other) noexcept {
    m_config = other.m_config;
    m_window = other.m_window;
    m_instance = other.m_instance;
    m_debugMessenger = other.m_debugMessenger;
    m_surface = other.m_surface;
    m_physicalDevice = other.m_physicalDevice;
    m_device = other.m_device;
    m_graphicsQueue = other.m_graphicsQueue;
    m_presentQueue = other.m_presentQueue;
    m_graphicsQueueIndex = other.m_graphicsQueueIndex;
    m_presentQueueIndex = other.m_presentQueueIndex;
    m_deviceProperties = other.m_deviceProperties;
    m_setObjectName = other.m_setObjectName;
    m_cmdBeginLabel = other.m_cmdBeginLabel;
    m_cmdEndLabel = other.m_cmdEndLabel;

    other.m_window = nullptr;
    other.m_instance = VK_NULL_HANDLE;
    other.m_debugMessenger = VK_NULL_HANDLE;
    other.m_surface = VK_NULL_HANDLE;
    other.m_physicalDevice = VK_NULL_HANDLE;
    other.m_device = VK_NULL_HANDLE;
    other.m_graphicsQueue = VK_NULL_HANDLE;
    other.m_presentQueue = VK_NULL_HANDLE;
    other.m_graphicsQueueIndex = 0U;
    other.m_presentQueueIndex = 0U;
    other.m_deviceProperties = {};
    other.m_setObjectName = nullptr;
    other.m_cmdBeginLabel = nullptr;
    other.m_cmdEndLabel = nullptr;
}

void VulkanContext::load_debug_functions() {
    if(!m_config.validation_enabled()) {
        return;
    }
    m_setObjectName = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
        vkGetInstanceProcAddr(m_instance, "vkSetDebugUtilsObjectNameEXT"));
    m_cmdBeginLabel = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(m_instance, "vkCmdBeginDebugUtilsLabelEXT"));
    m_cmdEndLabel = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(m_instance, "vkCmdEndDebugUtilsLabelEXT"));
}

} // namespace vulkano
