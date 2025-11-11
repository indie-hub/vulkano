#include <vulkano/vk/instance.hpp>

#include <GLFW/glfw3.h>

#include <cstdio>
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
#ifndef NDEBUG
constexpr bool kEnableValidationLayers {true};
#else
constexpr bool kEnableValidationLayers {false};
#endif

constexpr std::array<const char*, 1> kValidationLayers {"VK_LAYER_KHRONOS_validation"};

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT /*messageTypes*/, const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* /*userData*/) {
    const std::string_view message {callbackData != nullptr && callbackData->pMessage != nullptr
            ? callbackData->pMessage
            : "<no message>"};
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::fprintf(stderr, "[VULKAN] %s\n", std::string {message}.c_str());
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

bool check_validation_layer_support(const std::vector<const char*>& requestedLayers) {
    std::uint32_t layerCount {0U};
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : requestedLayers) {
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

std::vector<const char*> gather_required_instance_extensions(bool enableValidationLayers) {
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

void destroy_debug_utils_messenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger) noexcept {
    if (messenger == VK_NULL_HANDLE) {
        return;
    }
    const auto destroyFunction = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (destroyFunction != nullptr) {
        destroyFunction(instance, messenger, nullptr);
    }
}

VkInstance build_instance(bool enableValidationLayers, const std::vector<const char*>& validationLayers) {
    if (enableValidationLayers && !check_validation_layer_support(validationLayers)) {
        throw std::runtime_error {"Validation layers requested, but not available"};
    }

    VkApplicationInfo appInfo {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkano Renderer";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "Vulkano";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    const std::vector<const char*> extensions = gather_required_instance_extensions(enableValidationLayers);

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
} // namespace

namespace vulkano::vk {
Instance Instance::create() {
    const bool enableValidationLayers = kEnableValidationLayers;
    std::vector<const char*> validationLayers;
    if (enableValidationLayers) {
        validationLayers.assign(kValidationLayers.begin(), kValidationLayers.end());
    }

    VkInstance instance = build_instance(enableValidationLayers, validationLayers);

    VkDebugUtilsMessengerEXT debugMessenger {VK_NULL_HANDLE};
    if (enableValidationLayers) {
        VkDebugUtilsMessengerCreateInfoEXT messengerInfo {};
        populate_debug_messenger_create_info(messengerInfo);
        if (create_debug_utils_messenger(instance, &messengerInfo, &debugMessenger) != VK_SUCCESS) {
            vkDestroyInstance(instance, nullptr);
            throw std::runtime_error {"Failed to set up debug messenger"};
        }
    }

    return Instance {instance, debugMessenger, enableValidationLayers, validationLayers};
}

Instance::Instance(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, bool validationLayersEnabled,
    std::vector<const char*> validationLayers) noexcept
    : m_instance {instance}
    , m_debugMessenger {debugMessenger}
    , m_validationLayersEnabled {validationLayersEnabled}
    , m_validationLayers {std::move(validationLayers)} {
}

Instance::Instance(Instance&& other) noexcept
    : m_instance {other.m_instance}
    , m_debugMessenger {other.m_debugMessenger}
    , m_validationLayersEnabled {other.m_validationLayersEnabled}
    , m_validationLayers {std::move(other.m_validationLayers)} {
    other.m_instance = VK_NULL_HANDLE;
    other.m_debugMessenger = VK_NULL_HANDLE;
    other.m_validationLayersEnabled = false;
}

Instance& Instance::operator=(Instance&& other) noexcept {
    if (this != &other) {
        destroy_debug_utils_messenger(m_instance, m_debugMessenger);
        if (m_instance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_instance, nullptr);
        }
        m_instance = other.m_instance;
        m_debugMessenger = other.m_debugMessenger;
        m_validationLayersEnabled = other.m_validationLayersEnabled;
        m_validationLayers = std::move(other.m_validationLayers);

        other.m_instance = VK_NULL_HANDLE;
        other.m_debugMessenger = VK_NULL_HANDLE;
        other.m_validationLayersEnabled = false;
    }
    return *this;
}

Instance::~Instance() noexcept {
    destroy_debug_utils_messenger(m_instance, m_debugMessenger);
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

VkInstance Instance::handle() const noexcept {
    return m_instance;
}

VkDebugUtilsMessengerEXT Instance::debug_messenger() const noexcept {
    return m_debugMessenger;
}

bool Instance::validation_layers_enabled() const noexcept {
    return m_validationLayersEnabled;
}

const std::vector<const char*>& Instance::validation_layers() const noexcept {
    return m_validationLayers;
}
} // namespace vulkano::vk
