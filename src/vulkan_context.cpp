#include <vulkano/vulkan_context.hpp>
#include <vulkano/geometry.hpp>
#include <vulkano/camera.hpp>

#include <algorithm>
#include <cstddef>
#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/ext/matrix_transform.hpp>
// No experimental headers; build rotation from axis angles
#include <imgui.h>

#include <vulkano/imgui_overlay.hpp>

namespace vulkano {

namespace {
    constexpr const char* kValidationLayer {"VK_LAYER_KHRONOS_validation"};
    constexpr const char* kExtDebugUtils {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
    constexpr const char* kExtPortability {VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME};
    constexpr const char* kExtGetPhysProps2 {VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME};
    constexpr const char* kExtSwapchain {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    constexpr const char* kExtPortabilitySubset {"VK_KHR_portability_subset"};

    constexpr float kDefaultQueuePriority {1.0F};

    // Minimal 3D triangle using project Vertex format (pos3, normal3, uv2)
    constexpr std::array<Vertex, 3> kMeshVertices {
        Vertex {glm::vec3 {-0.5F, -0.5F, 0.0F}, glm::vec3 {0.0F, 0.0F, 1.0F}, glm::vec2 {0.0F, 0.0F}},
        Vertex {glm::vec3 { 0.5F, -0.5F, 0.0F}, glm::vec3 {0.0F, 0.0F, 1.0F}, glm::vec2 {1.0F, 0.0F}},
        Vertex {glm::vec3 { 0.0F,  0.5F, 0.0F}, glm::vec3 {0.0F, 0.0F, 1.0F}, glm::vec2 {0.5F, 1.0F}}
    };

    // Named clear values to avoid magic numbers in render logic
    constexpr std::array<float, 4> kClearColorBlack {0.0F, 0.0F, 0.0F, 1.0F};
    constexpr std::array<float, 4> kTriangleColorWhite {1.0F, 1.0F, 1.0F, 1.0F};
    constexpr float kClearDepthOne {1.0F};

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
    init_imgui(window);
    // Initialize camera with current aspect ratio if possible
    camera_ = std::make_unique<Camera>();
    if (swapchain_extent_.height != 0U) {
        const float aspect {static_cast<float>(swapchain_extent_.width) / static_cast<float>(swapchain_extent_.height)};
        camera_->set_aspect(aspect);
    }
    create_descriptor_set_layout();
    create_pipeline_layout();
    create_graphics_pipeline();
    create_depth_resources();
    // Build scene and upload geometry; fallback to single-triangle if failed
    create_scene();
    create_scene_buffers();
    if (vertex_buffer_ == VK_NULL_HANDLE) {
        create_vertex_buffer();
    }
    create_uniform_buffers_and_sets();
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

const std::string& VulkanContext::device_name() const noexcept {
    return device_name_cached_;
}

const VulkanContext::Light& VulkanContext::light() const noexcept {
    return light_;
}

void VulkanContext::set_light(const Light& l) noexcept {
    light_ = l;
}

std::size_t VulkanContext::primitive_count() const noexcept {
    return primitives_.size();
}

Primitive* VulkanContext::primitive_at(std::size_t index) noexcept {
    if (index >= primitives_.size()) {
        return nullptr;
    }
    return primitives_[index].get();
}

const Primitive* VulkanContext::primitive_at(std::size_t index) const noexcept {
    if (index >= primitives_.size()) {
        return nullptr;
    }
    return primitives_[index].get();
}

void VulkanContext::rebuild_scene_gpu_buffers() noexcept {
    create_scene_buffers();
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

    // Cache device name for UI
    VkPhysicalDeviceProperties props {};
    vkGetPhysicalDeviceProperties(best, &props);
    device_name_cached_ = props.deviceName != nullptr ? std::string {props.deviceName} : std::string {};

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

    // Load debug utils function pointers (device-level)
    if (validation_enabled_) {
        pfn_set_name_ = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetDeviceProcAddr(device_, "vkSetDebugUtilsObjectNameEXT"));
        pfn_cmd_begin_label_ = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetDeviceProcAddr(device_, "vkCmdBeginDebugUtilsLabelEXT"));
        pfn_cmd_end_label_ = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetDeviceProcAddr(device_, "vkCmdEndDebugUtilsLabelEXT"));
    }
    if (device_ != VK_NULL_HANDLE) {
        set_object_name(VK_OBJECT_TYPE_DEVICE, reinterpret_cast<std::uint64_t>(device_), "LogicalDevice");
    }
}

void VulkanContext::destroy() noexcept {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }
    if (imgui_) {
        imgui_->shutdown();
        imgui_.reset();
        imgui_ready_ = false;
    }
    destroy_sync_objects();
    destroy_command_pool_and_buffers();
    destroy_framebuffers();
    destroy_depth_resources();
    destroy_render_pass();
    destroy_pipeline();
    destroy_uniform_buffers_and_sets();
    destroy_descriptor_set_layout();
    destroy_index_buffer();
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
    // Layouts after device teardown are invalid; ensure destroyed earlier
    descriptor_sets_.clear();
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
    set_object_name(VK_OBJECT_TYPE_SWAPCHAIN_KHR, reinterpret_cast<std::uint64_t>(swapchain_), "Swapchain");
    swapchain_image_format_ = surfaceFormat.format;
    swapchain_extent_ = extent;
    if (camera_) {
        if (swapchain_extent_.height != 0U) {
            const float aspect {static_cast<float>(swapchain_extent_.width) / static_cast<float>(swapchain_extent_.height)};
            camera_->set_aspect(aspect);
        }
    }

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
            set_object_name(VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<std::uint64_t>(view), "SwapImageView");
        } else {
            swapchain_image_views_[i] = VK_NULL_HANDLE;
        }
    }
    // Recreate uniform buffers/descriptors to match swapchain image count
    destroy_uniform_buffers_and_sets();
    create_uniform_buffers_and_sets();
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

    // Choose depth format if not set
    if (depth_format_ == VK_FORMAT_UNDEFINED) {
        const VkFormat candidates[] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM };
        for (VkFormat fmt : candidates) {
            VkFormatProperties props {};
            vkGetPhysicalDeviceFormatProperties(physical_device_, fmt, &props);
            if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                depth_format_ = fmt;
                break;
            }
        }
        if (depth_format_ == VK_FORMAT_UNDEFINED) {
            depth_format_ = VK_FORMAT_D32_SFLOAT;
        }
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

    VkAttachmentDescription depthAttachment {};
    depthAttachment.format = depth_format_;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef {};
    colorAttachmentRef.attachment = 0U;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef {};
    depthAttachmentRef.attachment = 1U;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1U;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0U;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0U;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    const VkAttachmentDescription attachments[2] { colorAttachment, depthAttachment };

    VkRenderPassCreateInfo renderPassInfo {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 2U;
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1U;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1U;
    renderPassInfo.pDependencies = &dependency;

    VkRenderPass rp {VK_NULL_HANDLE};
    if (vkCreateRenderPass(device_, &renderPassInfo, nullptr, &rp) == VK_SUCCESS) {
        render_pass_ = rp;
        set_object_name(VK_OBJECT_TYPE_RENDER_PASS, reinterpret_cast<std::uint64_t>(render_pass_), "RenderPass");
    }
}

void VulkanContext::create_framebuffers() noexcept {
    if (device_ == VK_NULL_HANDLE || render_pass_ == VK_NULL_HANDLE || swapchain_extent_.width == 0U || swapchain_image_views_.empty()) {
        return;
    }
    framebuffers_.resize(swapchain_image_views_.size());
    for (std::size_t i {0U}; i < swapchain_image_views_.size(); ++i) {
        VkImageView attachments[2] {swapchain_image_views_[i], depth_image_view_};

        VkFramebufferCreateInfo fbInfo {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = render_pass_;
        fbInfo.attachmentCount = 2U;
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

void VulkanContext::create_depth_resources() noexcept {
    if (device_ == VK_NULL_HANDLE || physical_device_ == VK_NULL_HANDLE || depth_format_ == VK_FORMAT_UNDEFINED) {
        return;
    }
    // Destroy previous if any (defensive)
    destroy_depth_resources();

    VkImageCreateInfo imgInfo {};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.extent.width = swapchain_extent_.width;
    imgInfo.extent.height = swapchain_extent_.height;
    imgInfo.extent.depth = 1U;
    imgInfo.mipLevels = 1U;
    imgInfo.arrayLayers = 1U;
    imgInfo.format = depth_format_;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkImage image {VK_NULL_HANDLE};
    if (vkCreateImage(device_, &imgInfo, nullptr, &image) != VK_SUCCESS) {
        return;
    }

    VkMemoryRequirements memReq {};
    vkGetImageMemoryRequirements(device_, image, &memReq);
    const std::uint32_t typeIndex = [&]() noexcept {
        VkPhysicalDeviceMemoryProperties memProps {};
        vkGetPhysicalDeviceMemoryProperties(physical_device_, &memProps);
        for (std::uint32_t i {0U}; i < memProps.memoryTypeCount; ++i) {
            if ((memReq.memoryTypeBits & (1U << i)) != 0U &&
                (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                return i;
            }
        }
        return UINT32_MAX;
    }();
    if (typeIndex == UINT32_MAX) {
        vkDestroyImage(device_, image, nullptr);
        return;
    }

    VkMemoryAllocateInfo allocInfo {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = typeIndex;
    VkDeviceMemory memory {VK_NULL_HANDLE};
    if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyImage(device_, image, nullptr);
        return;
    }

    vkBindImageMemory(device_, image, memory, 0U);

    VkImageViewCreateInfo viewInfo {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depth_format_;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0U;
    viewInfo.subresourceRange.levelCount = 1U;
    viewInfo.subresourceRange.baseArrayLayer = 0U;
    viewInfo.subresourceRange.layerCount = 1U;
    VkImageView view {VK_NULL_HANDLE};
    if (vkCreateImageView(device_, &viewInfo, nullptr, &view) != VK_SUCCESS) {
        vkFreeMemory(device_, memory, nullptr);
        vkDestroyImage(device_, image, nullptr);
        return;
    }

    depth_image_ = image;
    depth_image_memory_ = memory;
    depth_image_view_ = view;
    set_object_name(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<std::uint64_t>(depth_image_), "DepthImage");
    set_object_name(VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<std::uint64_t>(depth_image_view_), "DepthImageView");
}

void VulkanContext::destroy_depth_resources() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (depth_image_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, depth_image_view_, nullptr);
        depth_image_view_ = VK_NULL_HANDLE;
    }
    if (depth_image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, depth_image_, nullptr);
        depth_image_ = VK_NULL_HANDLE;
    }
    if (depth_image_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, depth_image_memory_, nullptr);
        depth_image_memory_ = VK_NULL_HANDLE;
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
            if ((file_exists(dir + "/mesh.vert.spv") && file_exists(dir + "/mesh.frag.spv")) ||
                (file_exists(dir + "/triangle.vert.spv") && file_exists(dir + "/triangle.frag.spv"))) {
                return dir;
            }
        }
        // Prefer bin/shaders alongside binary when run from repo root
        if (file_exists("./bin/shaders/mesh.vert.spv") && file_exists("./bin/shaders/mesh.frag.spv")) {
            return std::string {"./bin/shaders"};
        }
        // Fallback to source shaders dir (in case of precompiled assets placed there)
        if (file_exists("./shaders/mesh.vert.spv") && file_exists("./shaders/mesh.frag.spv")) {
            return std::string {"./shaders"};
        }
        // Last resort: old triangle shaders
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
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    range.offset = 0U;
    // model(64) + baseColor(12) + shininess(4) = 80 bytes
    range.size = 80U;

    VkPipelineLayoutCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.setLayoutCount = (descriptor_set_layout_ != VK_NULL_HANDLE) ? 1U : 0U;
    info.pSetLayouts = (descriptor_set_layout_ != VK_NULL_HANDLE) ? &descriptor_set_layout_ : nullptr;
    info.pushConstantRangeCount = 1U;
    info.pPushConstantRanges = &range;

    VkPipelineLayout layout {VK_NULL_HANDLE};
    if (vkCreatePipelineLayout(device_, &info, nullptr, &layout) == VK_SUCCESS) {
        pipeline_layout_ = layout;
        set_object_name(VK_OBJECT_TYPE_PIPELINE_LAYOUT, reinterpret_cast<std::uint64_t>(pipeline_layout_), "PipelineLayout");
    }
}

void VulkanContext::create_graphics_pipeline() noexcept {
    if (device_ == VK_NULL_HANDLE || render_pass_ == VK_NULL_HANDLE || pipeline_layout_ == VK_NULL_HANDLE) {
        return;
    }
    const std::string dir {shader_dir_guess()};
    // Use new mesh shaders with pos3/normal3/uv2
    const std::vector<char> vertCode {read_file_binary(dir + "/mesh.vert.spv")};
    const std::vector<char> fragCode {read_file_binary(dir + "/mesh.frag.spv")};
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

    VkVertexInputAttributeDescription attribs[3] {};
    attribs[0].location = 0U; attribs[0].binding = 0U; attribs[0].format = VK_FORMAT_R32G32B32_SFLOAT; attribs[0].offset = 0U;
    attribs[1].location = 1U; attribs[1].binding = 0U; attribs[1].format = VK_FORMAT_R32G32B32_SFLOAT; attribs[1].offset = static_cast<std::uint32_t>(offsetof(Vertex, normal));
    attribs[2].location = 2U; attribs[2].binding = 0U; attribs[2].format = VK_FORMAT_R32G32_SFLOAT;      attribs[2].offset = static_cast<std::uint32_t>(offsetof(Vertex, uv));

    VkPipelineVertexInputStateCreateInfo vertexInput {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1U;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 3U;
    vertexInput.pVertexAttributeDescriptions = attribs;

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
    // Disable culling to avoid orientation/portability differences hiding the triangle.
    // We can tighten this later once verified visually across platforms.
    raster.cullMode = VK_CULL_MODE_NONE;
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

    VkPipelineDepthStencilStateCreateInfo depthStencil {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkGraphicsPipelineCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = 2U;
    info.pStages = stages;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &inputAssembly;
    info.pViewportState = &viewportState;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &msaa;
    info.pDepthStencilState = &depthStencil;
    info.pColorBlendState = &colorBlend;
    info.pDynamicState = nullptr;
    info.layout = pipeline_layout_;
    info.renderPass = render_pass_;
    info.subpass = 0U;

    VkPipeline pipeline {VK_NULL_HANDLE};
    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1U, &info, nullptr, &pipeline) == VK_SUCCESS) {
        graphics_pipeline_ = pipeline;
        set_object_name(VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<std::uint64_t>(graphics_pipeline_), "GraphicsPipeline");
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

bool VulkanContext::create_buffer(VkDeviceSize size,
                       VkBufferUsageFlags usage,
                       VkMemoryPropertyFlags properties,
                       VkBuffer& buffer,
                       VkDeviceMemory& memory,
                       const char* debugName) noexcept {
    if (device_ == VK_NULL_HANDLE || physical_device_ == VK_NULL_HANDLE || size == 0U) {
        return false;
    }
    VkBufferCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = size;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer buf {VK_NULL_HANDLE};
    if (vkCreateBuffer(device_, &info, nullptr, &buf) != VK_SUCCESS) {
        return false;
    }
    VkMemoryRequirements memReq {};
    vkGetBufferMemoryRequirements(device_, buf, &memReq);
    const std::uint32_t typeIndex {find_memory_type(physical_device_, memReq.memoryTypeBits, properties)};
    if (typeIndex == UINT32_MAX) {
        vkDestroyBuffer(device_, buf, nullptr);
        return false;
    }
    VkMemoryAllocateInfo alloc {};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = memReq.size;
    alloc.memoryTypeIndex = typeIndex;
    VkDeviceMemory mem {VK_NULL_HANDLE};
    if (vkAllocateMemory(device_, &alloc, nullptr, &mem) != VK_SUCCESS) {
        vkDestroyBuffer(device_, buf, nullptr);
        return false;
    }
    vkBindBufferMemory(device_, buf, mem, 0U);
    buffer = buf;
    memory = mem;
    if (debugName != nullptr) {
        set_object_name(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<std::uint64_t>(buffer), debugName);
    }
    return true;
}

bool VulkanContext::copy_buffer(VkBuffer src,
                     VkBuffer dst,
                     VkDeviceSize size) noexcept {
    if (device_ == VK_NULL_HANDLE || src == VK_NULL_HANDLE || dst == VK_NULL_HANDLE || size == 0U) {
        return false;
    }
    // Create a transient command pool/buffer for copy
    VkCommandPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphics_family_index_;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    VkCommandPool pool {VK_NULL_HANDLE};
    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        return false;
    }
    VkCommandBufferAllocateInfo allocInfo {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1U;
    VkCommandBuffer cmd {VK_NULL_HANDLE};
    if (vkAllocateCommandBuffers(device_, &allocInfo, &cmd) != VK_SUCCESS) {
        vkDestroyCommandPool(device_, pool, nullptr);
        return false;
    }
    VkCommandBufferBeginInfo begin {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);
    VkBufferCopy region {};
    region.srcOffset = 0U;
    region.dstOffset = 0U;
    region.size = size;
    vkCmdCopyBuffer(cmd, src, dst, 1U, &region);
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submit {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1U;
    submit.pCommandBuffers = &cmd;
    const VkResult subRes {vkQueueSubmit(graphics_queue_, 1U, &submit, VK_NULL_HANDLE)};
    if (subRes == VK_SUCCESS) {
        vkQueueWaitIdle(graphics_queue_);
    }
    vkFreeCommandBuffers(device_, pool, 1U, &cmd);
    vkDestroyCommandPool(device_, pool, nullptr);
    return subRes == VK_SUCCESS;
}

void VulkanContext::create_vertex_buffer() noexcept {
    if (device_ == VK_NULL_HANDLE || physical_device_ == VK_NULL_HANDLE) {
        return;
    }
    const VkDeviceSize bufferSize {static_cast<VkDeviceSize>(kMeshVertices.size() * sizeof(kMeshVertices[0]))};

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
        std::memcpy(data, kMeshVertices.data(), static_cast<std::size_t>(bufferSize));
        vkUnmapMemory(device_, memory);
    }

    vertex_buffer_ = buffer;
    vertex_buffer_memory_ = memory;
    set_object_name(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<std::uint64_t>(vertex_buffer_), "MeshVertexBuffer");
}

void VulkanContext::destroy_index_buffer() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (index_buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, index_buffer_, nullptr);
        index_buffer_ = VK_NULL_HANDLE;
    }
    if (index_buffer_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, index_buffer_memory_, nullptr);
        index_buffer_memory_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::create_scene() noexcept {
    // Construct default scene primitives with transforms/materials
    primitives_.clear();
    draw_ranges_.clear();
    // Plane: 10x10 at y=0, grey
    Plane::Params pp {};
    pp.width = 10.0F;
    pp.depth = 10.0F;
    pp.uvTiling = glm::vec2 {10.0F, 10.0F};
    auto plane = std::make_unique<Plane>(pp);
    Transform tPlane {};
    tPlane.position = glm::vec3 {0.0F, 0.0F, 0.0F};
    tPlane.scale = glm::vec3 {1.0F, 1.0F, 1.0F};
    plane->set_transform(tPlane);
    Material mPlane {};
    mPlane.baseColor = glm::vec3 {0.5F, 0.5F, 0.5F};
    mPlane.shininess = 16.0F;
    plane->set_material(mPlane);
    primitives_.push_back(std::move(plane));

    // Cube: scale 0.5, position (-1, 0.5, 0), warm orange
    auto cube = std::make_unique<Cube>();
    Transform tCube {};
    tCube.position = glm::vec3 {-1.0F, 0.5F, 0.0F};
    tCube.scale = glm::vec3 {0.5F, 0.5F, 0.5F};
    cube->set_transform(tCube);
    Material mCube {};
    mCube.baseColor = glm::vec3 {1.0F, 0.6F, 0.2F};
    mCube.shininess = 64.0F;
    cube->set_material(mCube);
    primitives_.push_back(std::move(cube));

    // Icosphere: scale 0.5, position (1, 0.5, 0), subdivisions=2, light blue
    Icosphere::Params sp {};
    sp.subdivisions = 2U;
    auto sphere = std::make_unique<Icosphere>(sp);
    Transform tSphere {};
    tSphere.position = glm::vec3 {1.0F, 0.5F, 0.0F};
    tSphere.scale = glm::vec3 {0.5F, 0.5F, 0.5F};
    sphere->set_transform(tSphere);
    Material mSphere {};
    mSphere.baseColor = glm::vec3 {0.6F, 0.8F, 1.0F};
    mSphere.shininess = 32.0F;
    sphere->set_material(mSphere);
    primitives_.push_back(std::move(sphere));
}

void VulkanContext::create_scene_buffers() noexcept {
    if (device_ == VK_NULL_HANDLE || physical_device_ == VK_NULL_HANDLE) {
        return;
    }
    if (primitives_.empty()) {
        return;
    }
    // Build combined vertex/index arrays
    std::vector<Vertex> allVerts {};
    std::vector<std::uint32_t> allIdx {};
    allVerts.reserve(4096U);
    allIdx.reserve(6144U);
    draw_ranges_.clear();
    std::uint32_t runningVertex {0U};
    std::uint32_t runningIndex {0U};
    for (const auto& p : primitives_) {
        if (!p) { continue; }
        const auto& verts = p->vertices();
        const auto& idx = p->indices();
        DrawRange dr {};
        dr.firstIndex = runningIndex;
        dr.firstVertex = runningVertex;
        dr.indexCount = static_cast<std::uint32_t>(idx.size());
        dr.prim = p.get();
        // Append vertices
        allVerts.insert(allVerts.end(), verts.begin(), verts.end());
        // Append indices with vertex offset applied
        for (std::uint32_t i : idx) {
            allIdx.push_back(i + runningVertex);
        }
        runningVertex += static_cast<std::uint32_t>(verts.size());
        runningIndex += static_cast<std::uint32_t>(idx.size());
        draw_ranges_.push_back(dr);
    }
    if (allVerts.empty() || allIdx.empty()) {
        return;
    }

    // Destroy previous buffers if any
    destroy_vertex_buffer();
    destroy_index_buffer();

    // Create staging buffers (host visible) and device-local buffers, then copy
    const VkDeviceSize vboSize {static_cast<VkDeviceSize>(allVerts.size() * sizeof(Vertex))};
    const VkDeviceSize iboSize {static_cast<VkDeviceSize>(allIdx.size() * sizeof(std::uint32_t))};

    VkBuffer stagingVBO {VK_NULL_HANDLE};
    VkDeviceMemory stagingVBOMem {VK_NULL_HANDLE};
    if (!create_buffer(vboSize,
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       stagingVBO, stagingVBOMem, "StagingVBO")) {
        return;
    }
    void* vData {nullptr};
    if (vkMapMemory(device_, stagingVBOMem, 0U, vboSize, 0U, &vData) == VK_SUCCESS) {
        std::memcpy(vData, allVerts.data(), static_cast<std::size_t>(vboSize));
        vkUnmapMemory(device_, stagingVBOMem);
    }

    VkBuffer stagingIBO {VK_NULL_HANDLE};
    VkDeviceMemory stagingIBOMem {VK_NULL_HANDLE};
    if (!create_buffer(iboSize,
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       stagingIBO, stagingIBOMem, "StagingIBO")) {
        vkDestroyBuffer(device_, stagingVBO, nullptr);
        vkFreeMemory(device_, stagingVBOMem, nullptr);
        return;
    }
    void* iData {nullptr};
    if (vkMapMemory(device_, stagingIBOMem, 0U, iboSize, 0U, &iData) == VK_SUCCESS) {
        std::memcpy(iData, allIdx.data(), static_cast<std::size_t>(iboSize));
        vkUnmapMemory(device_, stagingIBOMem);
    }

    VkBuffer deviceVBO {VK_NULL_HANDLE};
    VkDeviceMemory deviceVBOMem {VK_NULL_HANDLE};
    if (!create_buffer(vboSize,
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                       deviceVBO, deviceVBOMem, "SceneVertexBuffer")) {
        vkDestroyBuffer(device_, stagingVBO, nullptr);
        vkFreeMemory(device_, stagingVBOMem, nullptr);
        vkDestroyBuffer(device_, stagingIBO, nullptr);
        vkFreeMemory(device_, stagingIBOMem, nullptr);
        return;
    }

    VkBuffer deviceIBO {VK_NULL_HANDLE};
    VkDeviceMemory deviceIBOMem {VK_NULL_HANDLE};
    if (!create_buffer(iboSize,
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                       deviceIBO, deviceIBOMem, "SceneIndexBuffer")) {
        vkDestroyBuffer(device_, stagingVBO, nullptr);
        vkFreeMemory(device_, stagingVBOMem, nullptr);
        vkDestroyBuffer(device_, stagingIBO, nullptr);
        vkFreeMemory(device_, stagingIBOMem, nullptr);
        vkDestroyBuffer(device_, deviceVBO, nullptr);
        vkFreeMemory(device_, deviceVBOMem, nullptr);
        return;
    }

    // Copy from staging to device-local
    if (!copy_buffer(stagingVBO, deviceVBO, vboSize) || !copy_buffer(stagingIBO, deviceIBO, iboSize)) {
        // Cleanup on failure
        vkDestroyBuffer(device_, stagingVBO, nullptr);
        vkFreeMemory(device_, stagingVBOMem, nullptr);
        vkDestroyBuffer(device_, stagingIBO, nullptr);
        vkFreeMemory(device_, stagingIBOMem, nullptr);
        vkDestroyBuffer(device_, deviceVBO, nullptr);
        vkFreeMemory(device_, deviceVBOMem, nullptr);
        vkDestroyBuffer(device_, deviceIBO, nullptr);
        vkFreeMemory(device_, deviceIBOMem, nullptr);
        return;
    }

    // Free staging
    vkDestroyBuffer(device_, stagingVBO, nullptr);
    vkFreeMemory(device_, stagingVBOMem, nullptr);
    vkDestroyBuffer(device_, stagingIBO, nullptr);
    vkFreeMemory(device_, stagingIBOMem, nullptr);

    // Assign device-local buffers
    vertex_buffer_ = deviceVBO;
    vertex_buffer_memory_ = deviceVBOMem;
    index_buffer_ = deviceIBO;
    index_buffer_memory_ = deviceIBOMem;
}

void VulkanContext::create_descriptor_set_layout() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (descriptor_set_layout_ != VK_NULL_HANDLE) {
        return;
    }
    VkDescriptorSetLayoutBinding uboBinding {};
    uboBinding.binding = 0U;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1U;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    uboBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 1U;
    info.pBindings = &uboBinding;

    VkDescriptorSetLayout layout {VK_NULL_HANDLE};
    if (vkCreateDescriptorSetLayout(device_, &info, nullptr, &layout) == VK_SUCCESS) {
        descriptor_set_layout_ = layout;
        set_object_name(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, reinterpret_cast<std::uint64_t>(descriptor_set_layout_), "GlobalUBOLayout");
    }
}

namespace {
    struct GlobalUBO final {
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec3 cameraPos;
        float ambientStrength;
        glm::vec3 lightPos;
        float lightIntensity;
        glm::vec3 lightColor;
        float _pad0;
    };
}

void VulkanContext::create_uniform_buffers_and_sets() noexcept {
    if (device_ == VK_NULL_HANDLE || physical_device_ == VK_NULL_HANDLE || swapchain_images_.empty()) {
        return;
    }
    // Descriptor pool
    if (descriptor_pool_ == VK_NULL_HANDLE) {
        VkDescriptorPoolSize poolSize {};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = static_cast<std::uint32_t>(swapchain_images_.size());

        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1U;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = static_cast<std::uint32_t>(swapchain_images_.size());
        VkDescriptorPool pool {VK_NULL_HANDLE};
        if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &pool) == VK_SUCCESS) {
            descriptor_pool_ = pool;
            set_object_name(VK_OBJECT_TYPE_DESCRIPTOR_POOL, reinterpret_cast<std::uint64_t>(descriptor_pool_), "GlobalUBOPool");
        }
    }
    if (descriptor_pool_ == VK_NULL_HANDLE || descriptor_set_layout_ == VK_NULL_HANDLE) {
        return;
    }

    // Create uniform buffers
    const VkDeviceSize uboSize {static_cast<VkDeviceSize>(sizeof(GlobalUBO))};
    uniform_buffers_.resize(swapchain_images_.size());
    uniform_buffers_memory_.resize(swapchain_images_.size());

    for (std::size_t i {0U}; i < swapchain_images_.size(); ++i) {
        VkBufferCreateInfo bufInfo {};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = uboSize;
        bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkBuffer buffer {VK_NULL_HANDLE};
        if (vkCreateBuffer(device_, &bufInfo, nullptr, &buffer) != VK_SUCCESS) {
            continue;
        }
        VkMemoryRequirements memReq {};
        vkGetBufferMemoryRequirements(device_, buffer, &memReq);
        const std::uint32_t typeIndex {find_memory_type(physical_device_, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
        if (typeIndex == UINT32_MAX) {
            vkDestroyBuffer(device_, buffer, nullptr);
            continue;
        }
        VkMemoryAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = typeIndex;
        VkDeviceMemory memory {VK_NULL_HANDLE};
        if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
            vkDestroyBuffer(device_, buffer, nullptr);
            continue;
        }
        vkBindBufferMemory(device_, buffer, memory, 0U);
        uniform_buffers_[i] = buffer;
        uniform_buffers_memory_[i] = memory;
        set_object_name(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<std::uint64_t>(buffer), "GlobalUBOBuffer");
    }

    // Allocate descriptor sets
    descriptor_sets_.resize(swapchain_images_.size());
    std::vector<VkDescriptorSetLayout> layouts(swapchain_images_.size(), descriptor_set_layout_);
    VkDescriptorSetAllocateInfo allocInfo {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptor_pool_;
    allocInfo.descriptorSetCount = static_cast<std::uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();
    if (vkAllocateDescriptorSets(device_, &allocInfo, descriptor_sets_.data()) != VK_SUCCESS) {
        descriptor_sets_.clear();
        return;
    }

    for (std::size_t i {0U}; i < descriptor_sets_.size(); ++i) {
        VkDescriptorBufferInfo bufInfo {};
        bufInfo.buffer = uniform_buffers_[i];
        bufInfo.offset = 0U;
        bufInfo.range = sizeof(GlobalUBO);

        VkWriteDescriptorSet write {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptor_sets_[i];
        write.dstBinding = 0U;
        write.dstArrayElement = 0U;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1U;
        write.pBufferInfo = &bufInfo;
        write.pImageInfo = nullptr;
        write.pTexelBufferView = nullptr;
        vkUpdateDescriptorSets(device_, 1U, &write, 0U, nullptr);
    }
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
    for (std::size_t i {0U}; i < command_buffers_.size(); ++i) {
        if (command_buffers_[i] != VK_NULL_HANDLE) {
            set_object_name(VK_OBJECT_TYPE_COMMAND_BUFFER, reinterpret_cast<std::uint64_t>(command_buffers_[i]), "PrimaryCmdBuffer");
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

void VulkanContext::destroy_descriptor_set_layout() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
        descriptor_pool_ = VK_NULL_HANDLE;
    }
    if (descriptor_set_layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, descriptor_set_layout_, nullptr);
        descriptor_set_layout_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::destroy_uniform_buffers_and_sets() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    for (auto& buf : uniform_buffers_) {
        if (buf != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, buf, nullptr);
            buf = VK_NULL_HANDLE;
        }
    }
    for (auto& mem : uniform_buffers_memory_) {
        if (mem != VK_NULL_HANDLE) {
            vkFreeMemory(device_, mem, nullptr);
            mem = VK_NULL_HANDLE;
        }
    }
    uniform_buffers_.clear();
    uniform_buffers_memory_.clear();
    if (descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
        descriptor_pool_ = VK_NULL_HANDLE;
    }
    descriptor_sets_.clear();
}


bool VulkanContext::record_commands(std::uint32_t imageIndex) noexcept {
    if (device_ == VK_NULL_HANDLE || render_pass_ == VK_NULL_HANDLE || graphics_pipeline_ == VK_NULL_HANDLE || pipeline_layout_ == VK_NULL_HANDLE) {
        return false;
    }
    if (imageIndex >= command_buffers_.size() || imageIndex >= framebuffers_.size()) {
        return false;
    }

    VkCommandBuffer cmd {command_buffers_[imageIndex]};
    if (cmd == VK_NULL_HANDLE) {
        return false;
    }

    vkResetCommandBuffer(cmd, 0U);

    VkCommandBufferBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        return false;
    }

    VkClearValue clears[2] {};
    clears[0].color.float32[0] = kClearColorBlack[0];
    clears[0].color.float32[1] = kClearColorBlack[1];
    clears[0].color.float32[2] = kClearColorBlack[2];
    clears[0].color.float32[3] = kClearColorBlack[3];
    clears[1].depthStencil.depth = kClearDepthOne;
    clears[1].depthStencil.stencil = 0U;

    VkRenderPassBeginInfo rpInfo {};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = render_pass_;
    rpInfo.framebuffer = framebuffers_[imageIndex];
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = swapchain_extent_;
    rpInfo.clearValueCount = 2U;
    rpInfo.pClearValues = clears;
    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
    begin_label(cmd, "Frame");

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_);

    // Update and bind global UBO for this image
    if (imageIndex < uniform_buffers_memory_.size() && camera_) {
        GlobalUBO u {};
        u.view = camera_->view();
        u.proj = camera_->projection();
        u.cameraPos = camera_->position();
        u.ambientStrength = light_.ambient;
        u.lightPos = light_.position;
        u.lightIntensity = light_.intensity;
        u.lightColor = light_.color;
        u._pad0 = 0.0F;
        void* data {nullptr};
        if (vkMapMemory(device_, uniform_buffers_memory_[imageIndex], 0U, sizeof(GlobalUBO), 0U, &data) == VK_SUCCESS) {
            std::memcpy(data, &u, sizeof(GlobalUBO));
            vkUnmapMemory(device_, uniform_buffers_memory_[imageIndex]);
        }
    }
    if (imageIndex < descriptor_sets_.size()) {
        const VkDescriptorSet set {descriptor_sets_[imageIndex]};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0U, 1U, &set, 0U, nullptr);
    }

    // Compute camera matrices per-frame (not yet bound to shaders in this step)
    if (camera_) {
        (void)camera_->view_projection();
    }

    if (vertex_buffer_ != VK_NULL_HANDLE && !draw_ranges_.empty()) {
        const VkBuffer buffers[1] {vertex_buffer_};
        const VkDeviceSize offsets[1] {0U};
        vkCmdBindVertexBuffers(cmd, 0U, 1U, buffers, offsets);
        if (index_buffer_ != VK_NULL_HANDLE) {
            vkCmdBindIndexBuffer(cmd, index_buffer_, 0U, VK_INDEX_TYPE_UINT32);
        }

        struct PushConstants { glm::mat4 model; glm::vec3 baseColor; float shininess; } pc;
        for (const auto& dr : draw_ranges_) {
            if (dr.prim == nullptr) { continue; }
            const Transform& tr = dr.prim->transform();
            const Material& mat = dr.prim->material();
            glm::mat4 model {1.0F};
            model = glm::translate(model, tr.position);
            // Apply yaw (Y), pitch (X), roll (Z) rotations
            model = glm::rotate(model, tr.rotationEuler.y, glm::vec3 {0.0F, 1.0F, 0.0F});
            model = glm::rotate(model, tr.rotationEuler.x, glm::vec3 {1.0F, 0.0F, 0.0F});
            model = glm::rotate(model, tr.rotationEuler.z, glm::vec3 {0.0F, 0.0F, 1.0F});
            model = glm::scale(model, tr.scale);
            pc.model = model;
            pc.baseColor = mat.baseColor;
            pc.shininess = mat.shininess;
            vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0U, sizeof(PushConstants), &pc);
            if (index_buffer_ != VK_NULL_HANDLE) {
                vkCmdDrawIndexed(cmd, dr.indexCount, 1U, dr.firstIndex, 0, 0);
            } else {
                // Fallback to non-indexed draw if indices unavailable
                vkCmdDraw(cmd, dr.indexCount, 1U, dr.firstVertex, 0U);
            }
        }
    } else if (vertex_buffer_ != VK_NULL_HANDLE) {
        // Fallback single triangle vertex buffer
        const VkBuffer buffers[1] {vertex_buffer_};
        const VkDeviceSize offsets[1] {0U};
        vkCmdBindVertexBuffers(cmd, 0U, 1U, buffers, offsets);
        struct PushConstants { glm::mat4 model; glm::vec3 baseColor; float shininess; } pc;
        pc.model = glm::mat4(1.0F);
        pc.baseColor = glm::vec3(kTriangleColorWhite[0], kTriangleColorWhite[1], kTriangleColorWhite[2]);
        pc.shininess = 32.0F;
        vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0U, sizeof(PushConstants), &pc);
        vkCmdDraw(cmd, static_cast<std::uint32_t>(kMeshVertices.size()), 1U, 0U, 0U);
    }

    // Ensure ImGui draw data is ready and render overlay
    if (imgui_ready_) {
        ImGui::Render();
        if (ImGui::GetDrawData() != nullptr) {
            begin_label(cmd, "ImGui");
            imgui_->render(cmd);
            end_label(cmd);
        }
        imgui_frame_started_ = false;
    }

    vkCmdEndRenderPass(cmd);
    end_label(cmd);
    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        return false;
    }
    return true;
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

    const bool recorded {record_commands(imageIndex)};
    if (!recorded) {
        // Skip submit/present; let caller handle potential resize or resource init issue
        return false;
    }

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

void VulkanContext::recreate_swapchain(GLFWwindow* window) noexcept {
    if (device_ == VK_NULL_HANDLE || surface_ == VK_NULL_HANDLE) {
        return;
    }
    vkDeviceWaitIdle(device_);

    // Tear down resources that depend on swapchain extent/format
    destroy_command_pool_and_buffers();
    destroy_framebuffers();
    destroy_depth_resources();
    destroy_pipeline();
    destroy_render_pass();
    destroy_swapchain_and_views();

    // Recreate
    create_swapchain_and_views(window);
    create_render_pass();
    create_depth_resources();
    if (imgui_ready_) {
        // ImGui needs to be re-initialized with the new render pass
        const std::uint32_t minImages {static_cast<std::uint32_t>(swapchain_images_.size() > 1U ? swapchain_images_.size() : 2U)};
        imgui_->on_render_pass_changed(render_pass_, minImages);
    }
    // Pipeline layout (push constants) unchanged; reuse
    create_graphics_pipeline();
    create_framebuffers();
    create_command_pool_and_buffers();
}

void VulkanContext::init_imgui(GLFWwindow* window) noexcept {
    if (device_ == VK_NULL_HANDLE || physical_device_ == VK_NULL_HANDLE || render_pass_ == VK_NULL_HANDLE) {
        return;
    }
    imgui_ = std::make_unique<ImGuiOverlay>();
    const std::uint32_t minImages {static_cast<std::uint32_t>(swapchain_images_.size() > 1U ? swapchain_images_.size() : 2U)};
    imgui_->init(window, instance_, physical_device_, device_, graphics_family_index_, graphics_queue_, render_pass_, minImages);
    imgui_ready_ = true;
}

void VulkanContext::imgui_new_frame() noexcept {
    if (!imgui_ready_) {
        return;
    }
    imgui_->new_frame();
    imgui_frame_started_ = true;
}

// Reserved for future UI builder hook

void VulkanContext::set_object_name(VkObjectType type, std::uint64_t handle, const char* name) const noexcept {
    if (!validation_enabled_ || device_ == VK_NULL_HANDLE || pfn_set_name_ == nullptr || name == nullptr) {
        return;
    }
    VkDebugUtilsObjectNameInfoEXT info {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType = type;
    info.objectHandle = handle;
    info.pObjectName = name;
    (void)pfn_set_name_(device_, &info);
}

void VulkanContext::begin_label(VkCommandBuffer cmd, const char* name) const noexcept {
    if (!validation_enabled_ || cmd == VK_NULL_HANDLE || pfn_cmd_begin_label_ == nullptr || name == nullptr) {
        return;
    }
    VkDebugUtilsLabelEXT label {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = name;
    label.color[0] = 0.2F; label.color[1] = 0.6F; label.color[2] = 0.9F; label.color[3] = 1.0F;
    pfn_cmd_begin_label_(cmd, &label);
}

void VulkanContext::end_label(VkCommandBuffer cmd) const noexcept {
    if (!validation_enabled_ || cmd == VK_NULL_HANDLE || pfn_cmd_end_label_ == nullptr) {
        return;
    }
    pfn_cmd_end_label_(cmd);
}

} // namespace vulkano
