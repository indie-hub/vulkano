#include <vulkano/vulkan_context.hpp>
#include <vulkano/geometry.hpp>
#include <vulkano/camera.hpp>
#include <vulkano/textures.hpp>
#include <vulkano/config.hpp>

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>
#include <random>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/ext/matrix_transform.hpp>
// No experimental headers; build rotation from axis angles
#include <imgui.h>

#include <vulkano/imgui_overlay.hpp>
// Optional external image loading
#include <stb_image.h>

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

    // Push constants layout used by both VS/FS (matches shaders)
    struct PushConstants final {
        glm::mat4 model;
        glm::vec3 baseColor;
        float shininess;
        std::int32_t useAlbedoMap;
        std::int32_t useNormalMap;
        float normalStrength;
        float _pad1;
    };

    // Default SSAO UI settings (kept here to avoid magic numbers)
    constexpr bool kDefaultSsaoEnabled {false};
    constexpr std::int32_t kDefaultSsaoKernel {32};
    constexpr float kDefaultSsaoRadius {0.5F};
    constexpr float kDefaultSsaoBias {0.025F};
    constexpr float kDefaultSsaoPower {1.0F};
    constexpr bool kDefaultSsaoBlurEnabled {true};
    constexpr std::int32_t kDefaultSsaoBlurRadius {2};
    constexpr float kDefaultSsaoBlurSigma {1.5F};
    constexpr float kDefaultSsaoStrength {1.0F};
    constexpr std::uint32_t kDefaultSsaoNoiseSize {4U};

    struct SsaoParams final {
        glm::mat4 proj {1.0F};
        glm::mat4 invProj {1.0F};
        glm::vec2 noiseScale {1.0F, 1.0F};
        float radius {kDefaultSsaoRadius};
        float bias {kDefaultSsaoBias};
        float power {kDefaultSsaoPower};
        std::int32_t kernelSize {kDefaultSsaoKernel};
        glm::vec4 kernel[64] {};
    };

    [[nodiscard]] glm::vec3 rand_vec3_xy_hemisphere(std::mt19937& rng, std::uniform_real_distribution<float>& dist) noexcept {
        const float x {dist(rng) * 2.0F - 1.0F};
        const float y {dist(rng) * 2.0F - 1.0F};
        const float z {dist(rng)}; // [0,1]
        glm::vec3 v {x, y, z};
        const float len {std::sqrt(std::max(0.0F, v.x * v.x + v.y * v.y + v.z * v.z))};
        if (len > 0.0F) {
            v.x /= len; v.y /= len; v.z /= len;
        }
        return v;
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
    create_default_textures();
    if (vertex_buffer_ == VK_NULL_HANDLE) {
        create_vertex_buffer();
    }
    create_uniform_buffers_and_sets();
    create_framebuffers();
    create_command_pool_and_buffers();
    create_sync_objects();

    // Init SSAO settings with named defaults
    ssao_.enabled = kDefaultSsaoEnabled;
    ssao_.kernelSize = kDefaultSsaoKernel;
    ssao_.radius = kDefaultSsaoRadius;
    ssao_.bias = kDefaultSsaoBias;
    ssao_.power = kDefaultSsaoPower;
    ssao_.blurEnabled = kDefaultSsaoBlurEnabled;
    ssao_.blurRadius = kDefaultSsaoBlurRadius;
    ssao_.blurSigma = kDefaultSsaoBlurSigma;
    ssao_.aoStrength = kDefaultSsaoStrength;
    ssao_.noiseTexSize = kDefaultSsaoNoiseSize;

    // Prepare G-buffer and SSAO resources only when SSAO is enabled
    if (ssao_.enabled) {
        create_gbuffer_render_pass();
        create_gbuffer_resources();
        create_ssao_descriptor_set_layout();
        create_ssao_resources();
        create_ssao_render_pass();
        create_ssao_targets();
        create_ssao_pipeline();
        if (ssao_.blurEnabled) {
            create_blur_render_pass();
            create_blur_targets();
            create_blur_descriptor_set_layout();
            create_blur_resources();
            create_blur_pipeline();
        }
        // Compose uses swapchain render pass; build layout/pipeline now (resources only)
        create_compose_descriptor_set_layout();
        create_compose_pipeline();
    }
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

bool VulkanContext::sampler_anisotropy_supported() const noexcept {
    return sampler_anisotropy_supported_;
}

float VulkanContext::max_sampler_anisotropy() const noexcept {
    return max_sampler_anisotropy_;
}

std::uint32_t VulkanContext::albedo_width() const noexcept {
    return albedo_width_;
}

std::uint32_t VulkanContext::albedo_height() const noexcept {
    return albedo_height_;
}

std::uint32_t VulkanContext::albedo_mip_levels() const noexcept {
    return albedo_mip_levels_;
}

std::uint32_t VulkanContext::normal_width() const noexcept {
    return normal_width_;
}

std::uint32_t VulkanContext::normal_height() const noexcept {
    return normal_height_;
}

std::uint32_t VulkanContext::normal_mip_levels() const noexcept {
    return normal_mip_levels_;
}

const std::string& VulkanContext::albedo_label() const noexcept {
    return albedo_label_;
}

const std::string& VulkanContext::normal_label() const noexcept {
    return normal_label_;
}

VkFormat VulkanContext::albedo_format() const noexcept {
    return albedo_format_;
}

VkFormat VulkanContext::normal_format() const noexcept {
    return normal_format_;
}

namespace {
    [[nodiscard]] const char* format_to_string(VkFormat fmt) noexcept {
        switch (fmt) {
            case VK_FORMAT_R8G8B8A8_SRGB: { return "VK_FORMAT_R8G8B8A8_SRGB"; }
            case VK_FORMAT_R8G8B8A8_UNORM: { return "VK_FORMAT_R8G8B8A8_UNORM"; }
            default: { return "VK_FORMAT_UNKNOWN"; }
        }
    }
}

std::string VulkanContext::albedo_format_string() const noexcept {
    return std::string {format_to_string(albedo_format_)};
}

std::string VulkanContext::normal_format_string() const noexcept {
    return std::string {format_to_string(normal_format_)};
}

const VulkanContext::Light& VulkanContext::light() const noexcept {
    return light_;
}

void VulkanContext::set_light(const Light& l) noexcept {
    light_ = l;
}

const VulkanContext::SsaoSettings& VulkanContext::ssao_settings() const noexcept {
    return ssao_;
}

void VulkanContext::set_ssao_settings(const SsaoSettings& s) noexcept {
    // Clamp and sanitize values to safe ranges; avoid magic numbers via local named bounds
    const std::int32_t kMinKernel {16};
    const std::int32_t kMaxKernel {64};
    const float kMinRadius {0.05F};
    const float kMaxRadius {2.0F};
    const float kMinBias {0.001F};
    const float kMaxBias {0.1F};
    const float kMinPower {0.5F};
    const float kMaxPower {2.5F};
    const std::int32_t kMinBlurR {1};
    const std::int32_t kMaxBlurR {5};
    const float kMinSigma {0.5F};
    const float kMaxSigma {3.0F};
    const float kMinStrength {0.0F};
    const float kMaxStrength {1.5F};
    VulkanContext::SsaoSettings tmp {s};
    tmp.kernelSize = std::clamp(tmp.kernelSize, kMinKernel, kMaxKernel);
    if ((tmp.kernelSize % 16) != 0) {
        // Snap to nearest valid set {16, 32, 64}
        if (tmp.kernelSize < 24) { tmp.kernelSize = 16; }
        else if (tmp.kernelSize < 48) { tmp.kernelSize = 32; }
        else { tmp.kernelSize = 64; }
    }
    tmp.radius = std::clamp(tmp.radius, kMinRadius, kMaxRadius);
    tmp.bias = std::clamp(tmp.bias, kMinBias, kMaxBias);
    tmp.power = std::clamp(tmp.power, kMinPower, kMaxPower);
    tmp.blurRadius = std::clamp(tmp.blurRadius, kMinBlurR, kMaxBlurR);
    tmp.blurSigma = std::clamp(tmp.blurSigma, kMinSigma, kMaxSigma);
    tmp.aoStrength = std::clamp(tmp.aoStrength, kMinStrength, kMaxStrength);
    // Noise size fixed for now (tiled 4x4), keep from defaults
    tmp.noiseTexSize = kDefaultSsaoNoiseSize;
    ssao_ = tmp;
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

    // Cache device name for UI (deviceName is a fixed-size char array)
    VkPhysicalDeviceProperties props {};
    vkGetPhysicalDeviceProperties(best, &props);
    device_name_cached_ = std::string {props.deviceName};
    // Cache anisotropy capabilities (used later when creating samplers)
    max_sampler_anisotropy_ = props.limits.maxSamplerAnisotropy > 0.0F ? props.limits.maxSamplerAnisotropy : 1.0F;

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

    VkPhysicalDeviceFeatures supported {};
    vkGetPhysicalDeviceFeatures(physical_device_, &supported);

    VkPhysicalDeviceFeatures features {};
    // Enable sampler anisotropy if supported (MoltenVK supports it)
    if (supported.samplerAnisotropy == VK_TRUE) {
        features.samplerAnisotropy = VK_TRUE;
        sampler_anisotropy_supported_ = true;
    } else {
        sampler_anisotropy_supported_ = false;
    }

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
    destroy_gbuffer_resources();
    destroy_gbuffer_render_pass();
    destroy_ssao_resources();
    destroy_ssao_descriptor_set_layout();
    destroy_blur_pipeline();
    destroy_blur_targets();
    destroy_blur_render_pass();
    destroy_blur_resources();
    destroy_blur_descriptor_set_layout();
    destroy_compose_pipeline();
    destroy_compose_descriptor_set_layout();
    destroy_depth_resources();
    destroy_render_pass();
    destroy_pipeline();
    destroy_textures();
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
    // Name swapchain images for easier debugging
    if (validation_enabled_) {
        for (std::size_t i {0U}; i < swapchain_images_.size(); ++i) {
            char name[64];
            std::snprintf(name, sizeof(name), "SwapImage[%zu]", i);
            set_object_name(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<std::uint64_t>(swapchain_images_[i]), name);
        }
    }

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

    // Recreate offscreen resources only if SSAO is enabled
    destroy_ssao_resources();
    destroy_ssao_descriptor_set_layout();
    destroy_gbuffer_resources();
    destroy_gbuffer_render_pass();
    if (ssao_.enabled) {
        create_gbuffer_render_pass();
        create_gbuffer_resources();
        create_ssao_descriptor_set_layout();
        create_ssao_resources();
        create_ssao_render_pass();
        create_ssao_targets();
        create_ssao_pipeline();
        if (ssao_.blurEnabled) {
            create_blur_render_pass();
            create_blur_targets();
            create_blur_descriptor_set_layout();
            create_blur_resources();
            create_blur_pipeline();
        }
    }
}

void VulkanContext::destroy_swapchain_and_views() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    destroy_framebuffers();
    destroy_gbuffer_resources();
    destroy_gbuffer_render_pass();
    destroy_ssao_pipeline();
    destroy_ssao_targets();
    destroy_ssao_render_pass();
    destroy_ssao_resources();
    destroy_ssao_descriptor_set_layout();
    destroy_blur_pipeline();
    destroy_blur_targets();
    destroy_blur_render_pass();
    destroy_blur_resources();
    destroy_blur_descriptor_set_layout();
    destroy_compose_pipeline();
    destroy_compose_descriptor_set_layout();
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

void VulkanContext::create_gbuffer_render_pass() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    // Choose formats lazily if undefined
    if (gbuf_albedo_format_ == VK_FORMAT_UNDEFINED) {
        gbuf_albedo_format_ = VK_FORMAT_R8G8B8A8_SRGB;
    }
    if (gbuf_normal_format_ == VK_FORMAT_UNDEFINED) {
        // Prefer A2R10G10B10 UNORM; fallback to R16G16B16A16_SFLOAT
        VkFormat candidate {VK_FORMAT_A2R10G10B10_UNORM_PACK32};
        VkFormatProperties props {};
        vkGetPhysicalDeviceFormatProperties(physical_device_, candidate, &props);
        if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0U ||
            (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0U) {
            candidate = VK_FORMAT_R16G16B16A16_SFLOAT;
        }
        gbuf_normal_format_ = candidate;
    }
    if (gbuf_depth_format_ == VK_FORMAT_UNDEFINED) {
        // Reuse selected depth format when possible
        gbuf_depth_format_ = depth_format_ != VK_FORMAT_UNDEFINED ? depth_format_ : VK_FORMAT_D32_SFLOAT;
    }

    VkAttachmentDescription a0 {};
    a0.format = gbuf_albedo_format_;
    a0.samples = VK_SAMPLE_COUNT_1_BIT;
    a0.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    a0.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    a0.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    a0.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    a0.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    a0.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription a1 {};
    a1.format = gbuf_normal_format_;
    a1.samples = VK_SAMPLE_COUNT_1_BIT;
    a1.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    a1.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    a1.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    a1.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    a1.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    a1.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription ad {};
    ad.format = gbuf_depth_format_;
    ad.samples = VK_SAMPLE_COUNT_1_BIT;
    ad.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ad.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ad.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ad.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ad.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRefs[2];
    colorRefs[0].attachment = 0U;
    colorRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorRefs[1].attachment = 1U;
    colorRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference depthRef {};
    depthRef.attachment = 2U;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 2U;
    subpass.pColorAttachments = colorRefs;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep {};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0U;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0U;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription atts[3] {a0, a1, ad};
    VkRenderPassCreateInfo rp {};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp.attachmentCount = 3U;
    rp.pAttachments = atts;
    rp.subpassCount = 1U;
    rp.pSubpasses = &subpass;
    rp.dependencyCount = 1U;
    rp.pDependencies = &dep;
    VkRenderPass rpH {VK_NULL_HANDLE};
    if (vkCreateRenderPass(device_, &rp, nullptr, &rpH) == VK_SUCCESS) {
        gbuffer_render_pass_ = rpH;
        set_object_name(VK_OBJECT_TYPE_RENDER_PASS, reinterpret_cast<std::uint64_t>(gbuffer_render_pass_), "GBufferPass");
    }
}

void VulkanContext::destroy_gbuffer_render_pass() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (gbuffer_render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, gbuffer_render_pass_, nullptr);
        gbuffer_render_pass_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::create_ssao_render_pass() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (ao_raw_format_ == VK_FORMAT_UNDEFINED) {
        ao_raw_format_ = VK_FORMAT_R8_UNORM;
    }
    VkAttachmentDescription a0 {};
    a0.format = ao_raw_format_;
    a0.samples = VK_SAMPLE_COUNT_1_BIT;
    a0.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    a0.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    a0.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    a0.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    a0.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    a0.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef {};
    colorRef.attachment = 0U;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1U;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dep {};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0U;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0U;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rp {};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp.attachmentCount = 1U;
    rp.pAttachments = &a0;
    rp.subpassCount = 1U;
    rp.pSubpasses = &subpass;
    rp.dependencyCount = 1U;
    rp.pDependencies = &dep;
    VkRenderPass pass {VK_NULL_HANDLE};
    if (vkCreateRenderPass(device_, &rp, nullptr, &pass) == VK_SUCCESS) {
        ssao_render_pass_ = pass;
        set_object_name(VK_OBJECT_TYPE_RENDER_PASS, reinterpret_cast<std::uint64_t>(ssao_render_pass_), "SSAOPass");
    }
}

void VulkanContext::destroy_ssao_render_pass() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (ssao_render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, ssao_render_pass_, nullptr);
        ssao_render_pass_ = VK_NULL_HANDLE;
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
            // Name framebuffers for easier debugging
            if (validation_enabled_) {
                char name[64];
                std::snprintf(name, sizeof(name), "Framebuffer[%zu]", i);
                set_object_name(VK_OBJECT_TYPE_FRAMEBUFFER, reinterpret_cast<std::uint64_t>(framebuffers_[i]), name);
            }
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
    if (validation_enabled_) {
        set_object_name(VK_OBJECT_TYPE_DEVICE_MEMORY, reinterpret_cast<std::uint64_t>(memory), "DepthImageMemory");
    }

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
    // Keep size consistent with struct PushConstants
    range.size = static_cast<std::uint32_t>(sizeof(PushConstants));

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

void VulkanContext::create_ssao_pipeline() noexcept {
    if (device_ == VK_NULL_HANDLE || ssao_render_pass_ == VK_NULL_HANDLE || ssao_set_layout_ == VK_NULL_HANDLE) {
        return;
    }
    // Layout: only set 2, no push constants
    VkPipelineLayoutCreateInfo pl {};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount = 1U;
    const VkDescriptorSetLayout setLayouts[1] {ssao_set_layout_};
    pl.pSetLayouts = setLayouts;
    if (vkCreatePipelineLayout(device_, &pl, nullptr, &ssao_pipeline_layout_) != VK_SUCCESS) {
        ssao_pipeline_layout_ = VK_NULL_HANDLE;
        return;
    }
    set_object_name(VK_OBJECT_TYPE_PIPELINE_LAYOUT, reinterpret_cast<std::uint64_t>(ssao_pipeline_layout_), "SSAOPipelineLayout");

    const std::string dir {shader_dir_guess()};
    const std::vector<char> vertCode {read_file_binary(dir + "/ssao.vert.spv")};
    const std::vector<char> fragCode {read_file_binary(dir + "/ssao.frag.spv")};
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

    VkPipelineVertexInputStateCreateInfo vertexInput {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 0U;
    vertexInput.pVertexBindingDescriptions = nullptr;
    vertexInput.vertexAttributeDescriptionCount = 0U;
    vertexInput.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport {};
    viewport.x = 0.0F; viewport.y = 0.0F;
    viewport.width = static_cast<float>(swapchain_extent_.width);
    viewport.height = static_cast<float>(swapchain_extent_.height);
    viewport.minDepth = 0.0F; viewport.maxDepth = 1.0F;
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
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.depthBiasEnable = VK_FALSE;
    raster.lineWidth = 1.0F;

    VkPipelineMultisampleStateCreateInfo msaa {};
    msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState blendAtt {};
    blendAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
    blendAtt.blendEnable = VK_FALSE;
    VkPipelineColorBlendStateCreateInfo blend {};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.logicOpEnable = VK_FALSE;
    blend.attachmentCount = 1U;
    blend.pAttachments = &blendAtt;

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
    info.pColorBlendState = &blend;
    info.layout = ssao_pipeline_layout_;
    info.renderPass = ssao_render_pass_;
    info.subpass = 0U;

    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1U, &info, nullptr, &ssao_pipeline_) == VK_SUCCESS) {
        set_object_name(VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<std::uint64_t>(ssao_pipeline_), "SSAOPipeline");
    }
    vkDestroyShaderModule(device_, vertModule, nullptr);
    vkDestroyShaderModule(device_, fragModule, nullptr);
}

void VulkanContext::destroy_ssao_pipeline() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (ssao_pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, ssao_pipeline_, nullptr);
        ssao_pipeline_ = VK_NULL_HANDLE;
    }
    if (ssao_pipeline_layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, ssao_pipeline_layout_, nullptr);
        ssao_pipeline_layout_ = VK_NULL_HANDLE;
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

    VkVertexInputAttributeDescription attribs[4] {};
    attribs[0].location = 0U; attribs[0].binding = 0U; attribs[0].format = VK_FORMAT_R32G32B32_SFLOAT; attribs[0].offset = 0U;
    attribs[1].location = 1U; attribs[1].binding = 0U; attribs[1].format = VK_FORMAT_R32G32B32_SFLOAT; attribs[1].offset = static_cast<std::uint32_t>(offsetof(Vertex, normal));
    attribs[2].location = 2U; attribs[2].binding = 0U; attribs[2].format = VK_FORMAT_R32G32_SFLOAT;      attribs[2].offset = static_cast<std::uint32_t>(offsetof(Vertex, uv));
    attribs[3].location = 3U; attribs[3].binding = 0U; attribs[3].format = VK_FORMAT_R32G32B32_SFLOAT; attribs[3].offset = static_cast<std::uint32_t>(offsetof(Vertex, tangent));

    VkPipelineVertexInputStateCreateInfo vertexInput {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1U;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 4U;
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

bool VulkanContext::create_image_2d(std::uint32_t width,
                         std::uint32_t height,
                         std::uint32_t mipLevels,
                         VkFormat format,
                         VkImageTiling tiling,
                         VkImageUsageFlags usage,
                         VkMemoryPropertyFlags properties,
                         VkImage& image,
                         VkDeviceMemory& memory,
                         const char* debugName) noexcept {
    if (device_ == VK_NULL_HANDLE || physical_device_ == VK_NULL_HANDLE) {
        return false;
    }
    VkImageCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.extent.width = width;
    info.extent.height = height;
    info.extent.depth = 1U;
    info.mipLevels = mipLevels;
    info.arrayLayers = 1U;
    info.format = format;
    info.tiling = tiling;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    info.usage = usage;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkImage img {VK_NULL_HANDLE};
    if (vkCreateImage(device_, &info, nullptr, &img) != VK_SUCCESS) {
        return false;
    }
    VkMemoryRequirements memReq {};
    vkGetImageMemoryRequirements(device_, img, &memReq);
    const std::uint32_t typeIndex {find_memory_type(physical_device_, memReq.memoryTypeBits, properties)};
    if (typeIndex == UINT32_MAX) {
        vkDestroyImage(device_, img, nullptr);
        return false;
    }
    VkMemoryAllocateInfo alloc {};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = memReq.size;
    alloc.memoryTypeIndex = typeIndex;
    VkDeviceMemory mem {VK_NULL_HANDLE};
    if (vkAllocateMemory(device_, &alloc, nullptr, &mem) != VK_SUCCESS) {
        vkDestroyImage(device_, img, nullptr);
        return false;
    }
    vkBindImageMemory(device_, img, mem, 0U);
    image = img;
    memory = mem;
    if (debugName != nullptr) {
        set_object_name(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<std::uint64_t>(image), debugName);
    }
    return true;
}

bool VulkanContext::create_image_view_2d(VkImage image,
                              VkFormat format,
                              VkImageAspectFlags aspect,
                              std::uint32_t mipLevels,
                              VkImageView& view,
                              const char* debugName) noexcept {
    if (device_ == VK_NULL_HANDLE || image == VK_NULL_HANDLE) {
        return false;
    }
    VkImageViewCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image = image;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = format;
    info.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    info.subresourceRange.aspectMask = aspect;
    info.subresourceRange.baseMipLevel = 0U;
    info.subresourceRange.levelCount = mipLevels;
    info.subresourceRange.baseArrayLayer = 0U;
    info.subresourceRange.layerCount = 1U;
    VkImageView v {VK_NULL_HANDLE};
    if (vkCreateImageView(device_, &info, nullptr, &v) != VK_SUCCESS) {
        return false;
    }
    view = v;
    if (debugName != nullptr) {
        set_object_name(VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<std::uint64_t>(view), debugName);
    }
    return true;
}

void VulkanContext::transition_image_layout(VkImage image,
                                 VkFormat /*format*/,
                                 VkImageLayout oldLayout,
                                 VkImageLayout newLayout,
                                 std::uint32_t mipLevels) noexcept {
    if (device_ == VK_NULL_HANDLE || image == VK_NULL_HANDLE) {
        return;
    }
    VkCommandPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphics_family_index_;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    VkCommandPool pool {VK_NULL_HANDLE};
    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        return;
    }
    VkCommandBuffer cmd {VK_NULL_HANDLE};
    VkCommandBufferAllocateInfo allocInfo {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1U;
    if (vkAllocateCommandBuffers(device_, &allocInfo, &cmd) != VK_SUCCESS) {
        vkDestroyCommandPool(device_, pool, nullptr);
        return;
    }
    VkCommandBufferBeginInfo begin {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    VkImageMemoryBarrier barrier {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0U;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0U;
    barrier.subresourceRange.layerCount = 1U;

    VkPipelineStageFlags srcStage {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
    VkPipelineStageFlags dstStage {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0U;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        // Fallback: no-op
        barrier.srcAccessMask = 0U;
        barrier.dstAccessMask = 0U;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    vkCmdPipelineBarrier(cmd,
                         srcStage, dstStage,
                         0U,
                         0U, nullptr,
                         0U, nullptr,
                         1U, &barrier);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1U;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(graphics_queue_, 1U, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue_);
    vkFreeCommandBuffers(device_, pool, 1U, &cmd);
    vkDestroyCommandPool(device_, pool, nullptr);
}

bool VulkanContext::copy_buffer_to_image(VkBuffer buffer,
                              VkImage image,
                              std::uint32_t width,
                              std::uint32_t height) noexcept {
    if (device_ == VK_NULL_HANDLE || buffer == VK_NULL_HANDLE || image == VK_NULL_HANDLE) {
        return false;
    }
    VkCommandPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphics_family_index_;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    VkCommandPool pool {VK_NULL_HANDLE};
    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        return false;
    }
    VkCommandBuffer cmd {VK_NULL_HANDLE};
    VkCommandBufferAllocateInfo allocInfo {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1U;
    if (vkAllocateCommandBuffers(device_, &allocInfo, &cmd) != VK_SUCCESS) {
        vkDestroyCommandPool(device_, pool, nullptr);
        return false;
    }
    VkCommandBufferBeginInfo begin {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    VkBufferImageCopy region {};
    region.bufferOffset = 0U;
    region.bufferRowLength = 0U;
    region.bufferImageHeight = 0U;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0U;
    region.imageSubresource.baseArrayLayer = 0U;
    region.imageSubresource.layerCount = 1U;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1U};
    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1U, &region);
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

void VulkanContext::generate_mipmaps(VkImage image,
                          VkFormat imageFormat,
                          std::uint32_t texWidth,
                          std::uint32_t texHeight,
                          std::uint32_t mipLevels) noexcept {
    if (device_ == VK_NULL_HANDLE || image == VK_NULL_HANDLE || mipLevels <= 1U) {
        return;
    }
    VkFormatProperties props {};
    vkGetPhysicalDeviceFormatProperties(physical_device_, imageFormat, &props);
    if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) == 0) {
        // No linear blit support; transition whole image to shader-read and return
        transition_image_layout(image, imageFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels);
        return;
    }

    VkCommandPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphics_family_index_;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    VkCommandPool pool {VK_NULL_HANDLE};
    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        return;
    }
    VkCommandBuffer cmd {VK_NULL_HANDLE};
    VkCommandBufferAllocateInfo allocInfo {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1U;
    if (vkAllocateCommandBuffers(device_, &allocInfo, &cmd) != VK_SUCCESS) {
        vkDestroyCommandPool(device_, pool, nullptr);
        return;
    }
    VkCommandBufferBeginInfo begin {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    VkImageMemoryBarrier barrier {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0U;
    barrier.subresourceRange.layerCount = 1U;
    barrier.subresourceRange.levelCount = 1U;

    std::int32_t mipWidth = static_cast<std::int32_t>(texWidth);
    std::int32_t mipHeight = static_cast<std::int32_t>(texHeight);

    for (std::uint32_t i = 1; i < mipLevels; ++i) {
        barrier.subresourceRange.baseMipLevel = i - 1U;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0U,
                             0U, nullptr,
                             0U, nullptr,
                             1U, &barrier);

        VkImageBlit blit {};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1U;
        blit.srcSubresource.baseArrayLayer = 0U;
        blit.srcSubresource.layerCount = 1U;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0U;
        blit.dstSubresource.layerCount = 1U;

        vkCmdBlitImage(cmd,
                       image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1U, &blit,
                       VK_FILTER_LINEAR);

        // Transition previous level to shader read
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0U,
                             0U, nullptr,
                             0U, nullptr,
                             1U, &barrier);

        if (mipWidth > 1) { mipWidth /= 2; }
        if (mipHeight > 1) { mipHeight /= 2; }
    }

    // Transition last mip level to shader read
    barrier.subresourceRange.baseMipLevel = mipLevels - 1U;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0U,
                         0U, nullptr,
                         0U, nullptr,
                         1U, &barrier);

    vkEndCommandBuffer(cmd);
    VkSubmitInfo submit {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1U;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(graphics_queue_, 1U, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue_);
    vkFreeCommandBuffers(device_, pool, 1U, &cmd);
    vkDestroyCommandPool(device_, pool, nullptr);
}

void VulkanContext::create_default_textures() noexcept {
    if (device_ == VK_NULL_HANDLE || physical_device_ == VK_NULL_HANDLE) {
        return;
    }
    // If textures already created, skip
    if (albedo_image_ != VK_NULL_HANDLE && normal_image_ != VK_NULL_HANDLE) {
        return;
    }
    // Config constants for procedurals (fallbacks) centralized in config
    using vulkano::config::AlbedoSize;
    using vulkano::config::AlbedoSquares;
    using vulkano::config::AlbedoLight;
    using vulkano::config::AlbedoDark;
    using vulkano::config::NormalSize;
    using vulkano::config::NormalAmplitude;

    // Attempt external textures via stb_image when env variables are provided
    auto load_rgba8_from_file = [](const char* path, bool flipY) noexcept -> ImageRGBA8 {
        ImageRGBA8 out {};
        if (path == nullptr) {
            return out;
        }
        stbi_set_flip_vertically_on_load(flipY ? 1 : 0);
        int w {0};
        int h {0};
        int comp {0};
        unsigned char* data {stbi_load(path, &w, &h, &comp, STBI_rgb_alpha)};
        if (data == nullptr || w <= 0 || h <= 0) {
            if (data != nullptr) {
                stbi_image_free(data);
            }
            return out;
        }
        out.width = static_cast<std::uint32_t>(w);
        out.height = static_cast<std::uint32_t>(h);
        const std::size_t total {static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4U};
        out.pixels.assign(data, data + total);
        stbi_image_free(data);
        return out;
    };

    const char* albedoPath {std::getenv("VK_ALBEDO_PATH")};
    const char* normalPath {std::getenv("VK_NORMAL_PATH")};
    // Flip vertically by default for typical image conventions
    const bool flipY {true};

    ImageRGBA8 albedo {};
    ImageRGBA8 normal {};
    bool albedoFromFile {false};
    bool normalFromFile {false};
    if (albedoPath != nullptr && std::strlen(albedoPath) > 0U) {
        const ImageRGBA8 tmp {load_rgba8_from_file(albedoPath, flipY)};
        if (tmp.width != 0U && tmp.height != 0U && !tmp.pixels.empty()) {
            albedo = tmp;
            albedoFromFile = true;
        }
    }
    if (!albedoFromFile) {
        albedo = make_checkerboard_rgba(AlbedoSize, AlbedoSquares, AlbedoLight, AlbedoDark);
    }
    if (normalPath != nullptr && std::strlen(normalPath) > 0U) {
        const ImageRGBA8 tmp {load_rgba8_from_file(normalPath, flipY)};
        if (tmp.width != 0U && tmp.height != 0U && !tmp.pixels.empty()) {
            normal = tmp;
            normalFromFile = true;
        }
    }
    if (!normalFromFile) {
        normal = make_blue_noise_normal_rgba(NormalSize, NormalAmplitude);
    }

    // Helper to compute mip level count for a format/size (linear blit required for >1)
    auto compute_mip_levels = [&](VkFormat format, std::uint32_t width, std::uint32_t height) noexcept -> std::uint32_t {
        VkFormatProperties props {};
        vkGetPhysicalDeviceFormatProperties(physical_device_, format, &props);
        const bool canLinearBlit { (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0 };
        if (!canLinearBlit) {
            return 1U;
        }
        const std::uint32_t maxDim {width > height ? width : height};
        std::uint32_t m {0U};
        while ((1U << m) < maxDim) { ++m; }
        return m + 1U;
    };

    // Helper lambda to create, upload, mipmap, and view
    auto createTexture = [&](const ImageRGBA8& img, VkFormat format, VkImage& outImage, VkDeviceMemory& outMem, VkImageView& outView, VkSampler& outSampler, const char* baseName, bool enableAniso) {
        const std::uint32_t width {img.width};
        const std::uint32_t height {img.height};
        if (width == 0U || height == 0U || img.pixels.empty()) {
            return false;
        }
        const std::uint32_t mipLevels {compute_mip_levels(format, width, height)};

        const VkDeviceSize imageSize {static_cast<VkDeviceSize>(img.pixels.size())};
        VkBuffer staging {VK_NULL_HANDLE};
        VkDeviceMemory stagingMem {VK_NULL_HANDLE};
        if (!create_buffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem, "TexStaging")) {
            return false;
        }
        void* data {nullptr};
        if (vkMapMemory(device_, stagingMem, 0U, imageSize, 0U, &data) == VK_SUCCESS) {
            std::memcpy(data, img.pixels.data(), static_cast<std::size_t>(imageSize));
            vkUnmapMemory(device_, stagingMem);
        } else {
            vkDestroyBuffer(device_, staging, nullptr);
            vkFreeMemory(device_, stagingMem, nullptr);
            return false;
        }

        if (!create_image_2d(width, height, mipLevels, format, VK_IMAGE_TILING_OPTIMAL,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, outImage, outMem, baseName)) {
            vkDestroyBuffer(device_, staging, nullptr);
            vkFreeMemory(device_, stagingMem, nullptr);
            return false;
        }

        transition_image_layout(outImage, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);
        (void)copy_buffer_to_image(staging, outImage, width, height);
        if (mipLevels > 1U) {
            generate_mipmaps(outImage, format, width, height, mipLevels);
        } else {
            transition_image_layout(outImage, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels);
        }

        if (!create_image_view_2d(outImage, format, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, outView, baseName)) {
            vkDestroyBuffer(device_, staging, nullptr);
            vkFreeMemory(device_, stagingMem, nullptr);
            return false;
        }

        // Sampler
        VkSamplerCreateInfo samp {};
        samp.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samp.magFilter = VK_FILTER_LINEAR;
        samp.minFilter = VK_FILTER_LINEAR;
        samp.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samp.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samp.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samp.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samp.mipLodBias = 0.0F;
        if (enableAniso && sampler_anisotropy_supported_) {
            samp.anisotropyEnable = VK_TRUE;
            samp.maxAnisotropy = max_sampler_anisotropy_;
        } else {
            samp.anisotropyEnable = VK_FALSE;
            samp.maxAnisotropy = 1.0F;
        }
        samp.compareEnable = VK_FALSE;
        samp.compareOp = VK_COMPARE_OP_ALWAYS;
        samp.minLod = 0.0F;
        samp.maxLod = static_cast<float>(mipLevels);
        samp.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samp.unnormalizedCoordinates = VK_FALSE;
        VkSampler sampler {VK_NULL_HANDLE};
        if (vkCreateSampler(device_, &samp, nullptr, &sampler) != VK_SUCCESS) {
            vkDestroyBuffer(device_, staging, nullptr);
            vkFreeMemory(device_, stagingMem, nullptr);
            return false;
        }
        outSampler = sampler;
        if (validation_enabled_) {
            char name[64];
            std::snprintf(name, sizeof(name), "%sSampler", baseName);
            set_object_name(VK_OBJECT_TYPE_SAMPLER, reinterpret_cast<std::uint64_t>(outSampler), name);
        }

        vkDestroyBuffer(device_, staging, nullptr);
        vkFreeMemory(device_, stagingMem, nullptr);
        return true;
    };

    // Albedo (sRGB)
    albedo_width_ = albedo.width;
    albedo_height_ = albedo.height;
    albedo_mip_levels_ = compute_mip_levels(VK_FORMAT_R8G8B8A8_SRGB, albedo_width_, albedo_height_);
    albedo_format_ = VK_FORMAT_R8G8B8A8_SRGB;
    if (albedoFromFile) {
        albedo_label_ = std::string {"External ("} + (albedoPath != nullptr ? albedoPath : "?") + ")";
    } else {
        albedo_label_ = std::string {"Procedural Checkerboard"};
    }
    if (createTexture(albedo, VK_FORMAT_R8G8B8A8_SRGB, albedo_image_, albedo_image_memory_, albedo_image_view_, albedo_sampler_, "Albedo", true)) {
        // NOTE: mip levels set inside creator via sampler max LOD; keep cached level count conservative
    }
    // Normal (UNORM)
    normal_width_ = normal.width;
    normal_height_ = normal.height;
    normal_mip_levels_ = compute_mip_levels(VK_FORMAT_R8G8B8A8_UNORM, normal_width_, normal_height_);
    normal_format_ = VK_FORMAT_R8G8B8A8_UNORM;
    if (normalFromFile) {
        normal_label_ = std::string {"External ("} + (normalPath != nullptr ? normalPath : "?") + ")";
    } else {
        normal_label_ = std::string {"Procedural Blue-noise"};
    }
    (void)createTexture(normal, VK_FORMAT_R8G8B8A8_UNORM, normal_image_, normal_image_memory_, normal_image_view_, normal_sampler_, "Normal", false);
}

void VulkanContext::destroy_textures() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (albedo_sampler_ != VK_NULL_HANDLE) { vkDestroySampler(device_, albedo_sampler_, nullptr); albedo_sampler_ = VK_NULL_HANDLE; }
    if (albedo_image_view_ != VK_NULL_HANDLE) { vkDestroyImageView(device_, albedo_image_view_, nullptr); albedo_image_view_ = VK_NULL_HANDLE; }
    if (albedo_image_ != VK_NULL_HANDLE) { vkDestroyImage(device_, albedo_image_, nullptr); albedo_image_ = VK_NULL_HANDLE; }
    if (albedo_image_memory_ != VK_NULL_HANDLE) { vkFreeMemory(device_, albedo_image_memory_, nullptr); albedo_image_memory_ = VK_NULL_HANDLE; }

    if (normal_sampler_ != VK_NULL_HANDLE) { vkDestroySampler(device_, normal_sampler_, nullptr); normal_sampler_ = VK_NULL_HANDLE; }
    if (normal_image_view_ != VK_NULL_HANDLE) { vkDestroyImageView(device_, normal_image_view_, nullptr); normal_image_view_ = VK_NULL_HANDLE; }
    if (normal_image_ != VK_NULL_HANDLE) { vkDestroyImage(device_, normal_image_, nullptr); normal_image_ = VK_NULL_HANDLE; }
    if (normal_image_memory_ != VK_NULL_HANDLE) { vkFreeMemory(device_, normal_image_memory_, nullptr); normal_image_memory_ = VK_NULL_HANDLE; }
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
    if (validation_enabled_) {
        set_object_name(VK_OBJECT_TYPE_DEVICE_MEMORY, reinterpret_cast<std::uint64_t>(vertex_buffer_memory_), "MeshVertexBufferMemory");
    }
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
    VkDescriptorSetLayoutBinding bindings[3] {};
    // Binding 0: global UBO
    bindings[0].binding = 0U;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1U;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].pImmutableSamplers = nullptr;
    // Binding 1: albedo combined image sampler
    bindings[1].binding = 1U;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1U;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].pImmutableSamplers = nullptr;
    // Binding 2: normal combined image sampler
    bindings[2].binding = 2U;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1U;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 3U;
    info.pBindings = bindings;

    VkDescriptorSetLayout layout {VK_NULL_HANDLE};
    if (vkCreateDescriptorSetLayout(device_, &info, nullptr, &layout) == VK_SUCCESS) {
        descriptor_set_layout_ = layout;
        set_object_name(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, reinterpret_cast<std::uint64_t>(descriptor_set_layout_), "GlobalUBOLayout");
    }
}

void VulkanContext::create_ssao_descriptor_set_layout() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    VkDescriptorSetLayoutBinding b0 {};
    b0.binding = 0U;
    b0.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b0.descriptorCount = 1U;
    b0.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutBinding b1 {b0};
    b1.binding = 1U;
    VkDescriptorSetLayoutBinding b2 {b0};
    b2.binding = 2U;
    VkDescriptorSetLayoutBinding b3 {};
    b3.binding = 3U;
    b3.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    b3.descriptorCount = 1U;
    b3.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutBinding bindings[4] {b0, b1, b2, b3};
    VkDescriptorSetLayoutCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 4U;
    info.pBindings = bindings;
    VkDescriptorSetLayout layout {VK_NULL_HANDLE};
    if (vkCreateDescriptorSetLayout(device_, &info, nullptr, &layout) == VK_SUCCESS) {
        ssao_set_layout_ = layout;
        set_object_name(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, reinterpret_cast<std::uint64_t>(ssao_set_layout_), "SSAOSetLayout");
    }
}

void VulkanContext::destroy_ssao_descriptor_set_layout() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (ssao_set_layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, ssao_set_layout_, nullptr);
        ssao_set_layout_ = VK_NULL_HANDLE;
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
        VkDescriptorPoolSize poolSizes[2] {};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<std::uint32_t>(swapchain_images_.size());
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<std::uint32_t>(swapchain_images_.size() * 2U);

        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 2U;
        poolInfo.pPoolSizes = poolSizes;
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
        if (validation_enabled_) {
            set_object_name(VK_OBJECT_TYPE_DEVICE_MEMORY, reinterpret_cast<std::uint64_t>(memory), "GlobalUBOBufferMemory");
        }
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
    if (validation_enabled_) {
        for (std::size_t i {0U}; i < descriptor_sets_.size(); ++i) {
            char name[64];
            std::snprintf(name, sizeof(name), "GlobalUBOSet[%zu]", i);
            set_object_name(VK_OBJECT_TYPE_DESCRIPTOR_SET, reinterpret_cast<std::uint64_t>(descriptor_sets_[i]), name);
        }
    }

    for (std::size_t i {0U}; i < descriptor_sets_.size(); ++i) {
        VkDescriptorBufferInfo bufInfo {};
        bufInfo.buffer = uniform_buffers_[i];
        bufInfo.offset = 0U;
        bufInfo.range = sizeof(GlobalUBO);

        VkDescriptorImageInfo albedoInfo {};
        albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        albedoInfo.imageView = albedo_image_view_;
        albedoInfo.sampler = albedo_sampler_;

        VkDescriptorImageInfo normalInfo {};
        normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        normalInfo.imageView = normal_image_view_;
        normalInfo.sampler = normal_sampler_;

        VkWriteDescriptorSet writes[3] {};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = descriptor_sets_[i];
        writes[0].dstBinding = 0U;
        writes[0].dstArrayElement = 0U;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1U;
        writes[0].pBufferInfo = &bufInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = descriptor_sets_[i];
        writes[1].dstBinding = 1U;
        writes[1].dstArrayElement = 0U;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].descriptorCount = 1U;
        writes[1].pImageInfo = &albedoInfo;

        writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet = descriptor_sets_[i];
        writes[2].dstBinding = 2U;
        writes[2].dstArrayElement = 0U;
        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[2].descriptorCount = 1U;
        writes[2].pImageInfo = &normalInfo;

        vkUpdateDescriptorSets(device_, 3U, writes, 0U, nullptr);
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
            set_object_name(VK_OBJECT_TYPE_COMMAND_POOL, reinterpret_cast<std::uint64_t>(command_pool_), "PrimaryCommandPool");
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
        if (validation_enabled_ && image_available_semaphores_[i] != VK_NULL_HANDLE) {
            char name[64];
            std::snprintf(name, sizeof(name), "ImageAvailable[%u]", i);
            set_object_name(VK_OBJECT_TYPE_SEMAPHORE, reinterpret_cast<std::uint64_t>(image_available_semaphores_[i]), name);
        }
        vkCreateFence(device_, &fenceInfo, nullptr, &in_flight_fences_[i]);
        if (validation_enabled_ && in_flight_fences_[i] != VK_NULL_HANDLE) {
            char name[64];
            std::snprintf(name, sizeof(name), "InFlightFence[%u]", i);
            set_object_name(VK_OBJECT_TYPE_FENCE, reinterpret_cast<std::uint64_t>(in_flight_fences_[i]), name);
        }
    }

    // One render-finished semaphore per swapchain image to avoid reuse hazards across presents
    render_finished_semaphores_.assign(swapchain_images_.size(), VK_NULL_HANDLE);
    for (std::size_t i {0U}; i < swapchain_images_.size(); ++i) {
        vkCreateSemaphore(device_, &semInfo, nullptr, &render_finished_semaphores_[i]);
        if (validation_enabled_ && render_finished_semaphores_[i] != VK_NULL_HANDLE) {
            char name[64];
            std::snprintf(name, sizeof(name), "RenderFinished[%zu]", i);
            set_object_name(VK_OBJECT_TYPE_SEMAPHORE, reinterpret_cast<std::uint64_t>(render_finished_semaphores_[i]), name);
        }
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

void VulkanContext::create_gbuffer_resources() noexcept {
    if (device_ == VK_NULL_HANDLE || gbuffer_render_pass_ == VK_NULL_HANDLE) {
        return;
    }
    if (swapchain_extent_.width == 0U || swapchain_extent_.height == 0U) {
        return;
    }
    // Create images
    const std::uint32_t w {swapchain_extent_.width};
    const std::uint32_t h {swapchain_extent_.height};

    // Albedo
    create_image_2d(w, h, 1U, gbuf_albedo_format_, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    gbuf_albedo_image_, gbuf_albedo_memory_, "GbufAlbedoImage");
    create_image_view_2d(gbuf_albedo_image_, gbuf_albedo_format_, VK_IMAGE_ASPECT_COLOR_BIT, 1U, gbuf_albedo_view_, "GbufAlbedoView");

    // Normal (view-space packed)
    create_image_2d(w, h, 1U, gbuf_normal_format_, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    gbuf_normal_image_, gbuf_normal_memory_, "GbufNormalImage");
    create_image_view_2d(gbuf_normal_image_, gbuf_normal_format_, VK_IMAGE_ASPECT_COLOR_BIT, 1U, gbuf_normal_view_, "GbufNormalView");

    // Depth (sampled later by SSAO)
    create_image_2d(w, h, 1U, gbuf_depth_format_, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    gbuf_depth_image_, gbuf_depth_memory_, "GbufDepthImage");
    create_image_view_2d(gbuf_depth_image_, gbuf_depth_format_, VK_IMAGE_ASPECT_DEPTH_BIT, 1U, gbuf_depth_view_, "GbufDepthView");

    // Framebuffer
    VkImageView atts[3] {gbuf_albedo_view_, gbuf_normal_view_, gbuf_depth_view_};
    VkFramebufferCreateInfo fb {};
    fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb.renderPass = gbuffer_render_pass_;
    fb.attachmentCount = 3U;
    fb.pAttachments = atts;
    fb.width = w;
    fb.height = h;
    fb.layers = 1U;
    VkFramebuffer fbH {VK_NULL_HANDLE};
    if (vkCreateFramebuffer(device_, &fb, nullptr, &fbH) == VK_SUCCESS) {
        gbuffer_framebuffer_ = fbH;
        set_object_name(VK_OBJECT_TYPE_FRAMEBUFFER, reinterpret_cast<std::uint64_t>(gbuffer_framebuffer_), "GBufferFB");
    }
}

void VulkanContext::destroy_gbuffer_resources() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (gbuffer_framebuffer_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device_, gbuffer_framebuffer_, nullptr);
        gbuffer_framebuffer_ = VK_NULL_HANDLE;
    }
    if (gbuf_albedo_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, gbuf_albedo_view_, nullptr);
        gbuf_albedo_view_ = VK_NULL_HANDLE;
    }
    if (gbuf_albedo_image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, gbuf_albedo_image_, nullptr);
        gbuf_albedo_image_ = VK_NULL_HANDLE;
    }
    if (gbuf_albedo_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, gbuf_albedo_memory_, nullptr);
        gbuf_albedo_memory_ = VK_NULL_HANDLE;
    }
    if (gbuf_normal_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, gbuf_normal_view_, nullptr);
        gbuf_normal_view_ = VK_NULL_HANDLE;
    }
    if (gbuf_normal_image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, gbuf_normal_image_, nullptr);
        gbuf_normal_image_ = VK_NULL_HANDLE;
    }
    if (gbuf_normal_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, gbuf_normal_memory_, nullptr);
        gbuf_normal_memory_ = VK_NULL_HANDLE;
    }
    if (gbuf_depth_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, gbuf_depth_view_, nullptr);
        gbuf_depth_view_ = VK_NULL_HANDLE;
    }
    if (gbuf_depth_image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, gbuf_depth_image_, nullptr);
        gbuf_depth_image_ = VK_NULL_HANDLE;
    }
    if (gbuf_depth_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, gbuf_depth_memory_, nullptr);
        gbuf_depth_memory_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::create_ssao_targets() noexcept {
    if (device_ == VK_NULL_HANDLE || ssao_render_pass_ == VK_NULL_HANDLE) {
        return;
    }
    if (swapchain_extent_.width == 0U || swapchain_extent_.height == 0U) {
        return;
    }
    const std::uint32_t w {swapchain_extent_.width};
    const std::uint32_t h {swapchain_extent_.height};
    create_image_2d(w, h, 1U, ao_raw_format_, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    ao_raw_image_, ao_raw_memory_, "AoRawImage");
    create_image_view_2d(ao_raw_image_, ao_raw_format_, VK_IMAGE_ASPECT_COLOR_BIT, 1U, ao_raw_view_, "AoRawView");

    VkImageView atts[1] {ao_raw_view_};
    VkFramebufferCreateInfo fb {};
    fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb.renderPass = ssao_render_pass_;
    fb.attachmentCount = 1U;
    fb.pAttachments = atts;
    fb.width = w;
    fb.height = h;
    fb.layers = 1U;
    VkFramebuffer fbH {VK_NULL_HANDLE};
    if (vkCreateFramebuffer(device_, &fb, nullptr, &fbH) == VK_SUCCESS) {
        ssao_framebuffer_ = fbH;
        set_object_name(VK_OBJECT_TYPE_FRAMEBUFFER, reinterpret_cast<std::uint64_t>(ssao_framebuffer_), "SSAO_FB");
    }
}

void VulkanContext::destroy_ssao_targets() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (ssao_framebuffer_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device_, ssao_framebuffer_, nullptr);
        ssao_framebuffer_ = VK_NULL_HANDLE;
    }
    if (ao_raw_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, ao_raw_view_, nullptr);
        ao_raw_view_ = VK_NULL_HANDLE;
    }
    if (ao_raw_image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, ao_raw_image_, nullptr);
        ao_raw_image_ = VK_NULL_HANDLE;
    }
    if (ao_raw_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, ao_raw_memory_, nullptr);
        ao_raw_memory_ = VK_NULL_HANDLE;
    }
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

void VulkanContext::create_ssao_resources() noexcept {
    if (device_ == VK_NULL_HANDLE || ssao_set_layout_ == VK_NULL_HANDLE) {
        return;
    }
    if (swapchain_extent_.width == 0U || swapchain_extent_.height == 0U) {
        return;
    }
    const std::uint32_t imageCount {static_cast<std::uint32_t>(swapchain_images_.size())};

    // Create noise texture (4x4) in RGBA8 UNORM with XY in [0,1]
    if (ssao_noise_image_ == VK_NULL_HANDLE) {
        const std::uint32_t n {ssao_.noiseTexSize};
        const std::size_t pixelCount {static_cast<std::size_t>(n) * static_cast<std::size_t>(n)};
        std::vector<std::uint8_t> pixels(pixelCount * 4U, 255U);
        std::mt19937 rng {1234567U};
        std::uniform_real_distribution<float> dist {0.0F, 1.0F};
        for (std::uint32_t y {0U}; y < n; ++y) {
            for (std::uint32_t x {0U}; x < n; ++x) {
                const std::size_t idx {static_cast<std::size_t>((y * n + x) * 4U)};
                const float rx {-1.0F + 2.0F * dist(rng)};
                const float ry {-1.0F + 2.0F * dist(rng)};
                const std::uint8_t r {static_cast<std::uint8_t>(std::clamp((rx * 0.5F + 0.5F) * 255.0F, 0.0F, 255.0F))};
                const std::uint8_t g {static_cast<std::uint8_t>(std::clamp((ry * 0.5F + 0.5F) * 255.0F, 0.0F, 255.0F))};
                pixels[idx + 0U] = r;
                pixels[idx + 1U] = g;
                pixels[idx + 2U] = 128U;
                pixels[idx + 3U] = 255U;
            }
        }

        const VkDeviceSize size {static_cast<VkDeviceSize>(pixels.size())};
        VkBuffer staging {VK_NULL_HANDLE};
        VkDeviceMemory stagingMem {VK_NULL_HANDLE};
        if (create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          staging, stagingMem, "SsaoNoiseStaging")) {
            void* data {nullptr};
            if (vkMapMemory(device_, stagingMem, 0U, size, 0U, &data) == VK_SUCCESS) {
                std::memcpy(data, pixels.data(), pixels.size());
                vkUnmapMemory(device_, stagingMem);
            }
            create_image_2d(n, n, 1U, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                            ssao_noise_image_, ssao_noise_memory_, "SsaoNoiseImage");
            transition_image_layout(ssao_noise_image_, VK_FORMAT_R8G8B8A8_UNORM,
                                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1U);
            copy_buffer_to_image(staging, ssao_noise_image_, n, n);
            transition_image_layout(ssao_noise_image_, VK_FORMAT_R8G8B8A8_UNORM,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1U);
            create_image_view_2d(ssao_noise_image_, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1U, ssao_noise_view_, "SsaoNoiseView");
            if (staging != VK_NULL_HANDLE) { vkDestroyBuffer(device_, staging, nullptr); }
            if (stagingMem != VK_NULL_HANDLE) { vkFreeMemory(device_, stagingMem, nullptr); }
        }
        // Noise sampler (repeat)
        VkSamplerCreateInfo si {};
        si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        si.magFilter = VK_FILTER_NEAREST;
        si.minFilter = VK_FILTER_NEAREST;
        si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        si.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        si.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        si.maxAnisotropy = 1.0F;
        si.maxLod = 0.0F;
        vkCreateSampler(device_, &si, nullptr, &ssao_noise_sampler_);
        if (ssao_noise_sampler_ != VK_NULL_HANDLE) {
            set_object_name(VK_OBJECT_TYPE_SAMPLER, reinterpret_cast<std::uint64_t>(ssao_noise_sampler_), "SsaoNoiseSampler");
        }
        // Clamp sampler for depth/normal
        VkSamplerCreateInfo sci {si};
        sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.magFilter = VK_FILTER_LINEAR;
        sci.minFilter = VK_FILTER_LINEAR;
        vkCreateSampler(device_, &sci, nullptr, &ssao_clamp_sampler_);
        if (ssao_clamp_sampler_ != VK_NULL_HANDLE) {
            set_object_name(VK_OBJECT_TYPE_SAMPLER, reinterpret_cast<std::uint64_t>(ssao_clamp_sampler_), "SsaoClampSampler");
        }
    }

    // Create SSAO UBOs per swapchain image
    ssao_uniform_buffers_.resize(imageCount);
    ssao_uniform_memory_.resize(imageCount);
    for (std::uint32_t i {0U}; i < imageCount; ++i) {
        if (ssao_uniform_buffers_[i] == VK_NULL_HANDLE) {
            create_buffer(static_cast<VkDeviceSize>(sizeof(SsaoParams)),
                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          ssao_uniform_buffers_[i], ssao_uniform_memory_[i], "SsaoUBO");
        }
        SsaoParams p {};
        if (camera_) {
            p.proj = camera_->projection();
            p.invProj = glm::inverse(p.proj);
        }
        p.noiseScale = glm::vec2 {static_cast<float>(swapchain_extent_.width) / static_cast<float>(ssao_.noiseTexSize),
                                   static_cast<float>(swapchain_extent_.height) / static_cast<float>(ssao_.noiseTexSize)};
        p.radius = ssao_.radius;
        p.bias = ssao_.bias;
        p.power = ssao_.power;
        p.kernelSize = ssao_.kernelSize;
        std::mt19937 rng {i + 42U};
        std::uniform_real_distribution<float> dist {0.0F, 1.0F};
        for (int k {0}; k < p.kernelSize && k < 64; ++k) {
            glm::vec3 sample = rand_vec3_xy_hemisphere(rng, dist);
            float scale = static_cast<float>(k) / static_cast<float>(p.kernelSize);
            const float scaleLerp = 0.1F + 0.9F * (scale * scale);
            sample *= scaleLerp;
            p.kernel[k] = glm::vec4 {sample, 0.0F};
        }
        void* data {nullptr};
        if (vkMapMemory(device_, ssao_uniform_memory_[i], 0U, sizeof(SsaoParams), 0U, &data) == VK_SUCCESS) {
            std::memcpy(data, &p, sizeof(SsaoParams));
            vkUnmapMemory(device_, ssao_uniform_memory_[i]);
        }
    }

    // Descriptor pool for SSAO
    if (ssao_descriptor_pool_ == VK_NULL_HANDLE) {
        VkDescriptorPoolSize poolSizes[2] {};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[0].descriptorCount = imageCount * 3U;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[1].descriptorCount = imageCount;
        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = imageCount;
        poolInfo.poolSizeCount = 2U;
        poolInfo.pPoolSizes = poolSizes;
        vkCreateDescriptorPool(device_, &poolInfo, nullptr, &ssao_descriptor_pool_);
    }

    // Allocate and update descriptor sets
    ssao_descriptor_sets_.assign(imageCount, VK_NULL_HANDLE);
    if (ssao_descriptor_pool_ != VK_NULL_HANDLE) {
        std::vector<VkDescriptorSetLayout> layouts(imageCount, ssao_set_layout_);
        VkDescriptorSetAllocateInfo alloc {};
        alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc.descriptorPool = ssao_descriptor_pool_;
        alloc.descriptorSetCount = imageCount;
        alloc.pSetLayouts = layouts.data();
        const VkResult allocRes {vkAllocateDescriptorSets(device_, &alloc, ssao_descriptor_sets_.data())};
        if (allocRes != VK_SUCCESS) {
            return;
        }
        for (std::uint32_t i {0U}; i < imageCount; ++i) {
            if (ssao_descriptor_sets_[i] == VK_NULL_HANDLE) {
                continue;
            }
            VkDescriptorImageInfo depthInfo {};
            depthInfo.sampler = ssao_clamp_sampler_;
            depthInfo.imageView = gbuf_depth_view_;
            depthInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkDescriptorImageInfo normalInfo {};
            normalInfo.sampler = ssao_clamp_sampler_;
            normalInfo.imageView = gbuf_normal_view_;
            normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkDescriptorImageInfo noiseInfo {};
            noiseInfo.sampler = ssao_noise_sampler_;
            noiseInfo.imageView = ssao_noise_view_;
            noiseInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkDescriptorBufferInfo bufInfo {};
            bufInfo.buffer = ssao_uniform_buffers_[i];
            bufInfo.offset = 0U;
            bufInfo.range = sizeof(SsaoParams);
            VkWriteDescriptorSet writes[4] {};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = ssao_descriptor_sets_[i];
            writes[0].dstBinding = 0U;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[0].descriptorCount = 1U;
            writes[0].pImageInfo = &depthInfo;
            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = ssao_descriptor_sets_[i];
            writes[1].dstBinding = 1U;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].descriptorCount = 1U;
            writes[1].pImageInfo = &normalInfo;
            writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[2].dstSet = ssao_descriptor_sets_[i];
            writes[2].dstBinding = 2U;
            writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[2].descriptorCount = 1U;
            writes[2].pImageInfo = &noiseInfo;
            writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[3].dstSet = ssao_descriptor_sets_[i];
            writes[3].dstBinding = 3U;
            writes[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[3].descriptorCount = 1U;
            writes[3].pBufferInfo = &bufInfo;
            vkUpdateDescriptorSets(device_, 4U, writes, 0U, nullptr);
        }
    }
}

void VulkanContext::destroy_ssao_resources() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (ssao_descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, ssao_descriptor_pool_, nullptr);
        ssao_descriptor_pool_ = VK_NULL_HANDLE;
    }
    ssao_descriptor_sets_.clear();
    for (auto& b : ssao_uniform_buffers_) {
        if (b != VK_NULL_HANDLE) { vkDestroyBuffer(device_, b, nullptr); b = VK_NULL_HANDLE; }
    }
    for (auto& m : ssao_uniform_memory_) {
        if (m != VK_NULL_HANDLE) { vkFreeMemory(device_, m, nullptr); m = VK_NULL_HANDLE; }
    }
    if (ssao_noise_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, ssao_noise_view_, nullptr);
        ssao_noise_view_ = VK_NULL_HANDLE;
    }
    if (ssao_noise_image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, ssao_noise_image_, nullptr);
        ssao_noise_image_ = VK_NULL_HANDLE;
    }
    if (ssao_noise_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, ssao_noise_memory_, nullptr);
        ssao_noise_memory_ = VK_NULL_HANDLE;
    }
    if (ssao_noise_sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, ssao_noise_sampler_, nullptr);
        ssao_noise_sampler_ = VK_NULL_HANDLE;
    }
    if (ssao_clamp_sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, ssao_clamp_sampler_, nullptr);
        ssao_clamp_sampler_ = VK_NULL_HANDLE;
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

        PushConstants pc {};
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
            pc.useAlbedoMap = (mat.useAlbedo ? 1 : 0);
            pc.useNormalMap = (mat.useNormal ? 1 : 0);
            pc.normalStrength = std::clamp(mat.normalStrength, 0.0F, 2.0F);
            pc._pad1 = 0.0F;
            begin_label(cmd, dr.prim->type_name());
            vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0U, sizeof(PushConstants), &pc);
            if (index_buffer_ != VK_NULL_HANDLE) {
                vkCmdDrawIndexed(cmd, dr.indexCount, 1U, dr.firstIndex, 0, 0);
            } else {
                // Fallback to non-indexed draw if indices unavailable
                vkCmdDraw(cmd, dr.indexCount, 1U, dr.firstVertex, 0U);
            }
            end_label(cmd);
        }
    } else if (vertex_buffer_ != VK_NULL_HANDLE) {
        // Fallback single triangle vertex buffer
        const VkBuffer buffers[1] {vertex_buffer_};
        const VkDeviceSize offsets[1] {0U};
        vkCmdBindVertexBuffers(cmd, 0U, 1U, buffers, offsets);
        PushConstants pc {};
        pc.model = glm::mat4(1.0F);
        pc.baseColor = glm::vec3(kTriangleColorWhite[0], kTriangleColorWhite[1], kTriangleColorWhite[2]);
        pc.shininess = 32.0F;
        pc.useAlbedoMap = 1;
        pc.useNormalMap = 1;
        pc.normalStrength = 1.0F;
        pc._pad1 = 0.0F;
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
    // Rebuild sync objects as swapchain image count can change across recreations
    destroy_sync_objects();
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
    create_sync_objects();
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

void VulkanContext::imgui_end_frame_build() noexcept {
    if (!imgui_ready_ || !imgui_frame_started_) {
        return;
    }
    imgui_->end_frame_build();
    imgui_frame_started_ = false;
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

// --- Camera interaction helpers ---
void VulkanContext::camera_orbit_delta(float dYaw, float dPitch) noexcept {
    if (!camera_) {
        return;
    }
    camera_->orbit_delta(dYaw, dPitch, 0.0F);
}

void VulkanContext::camera_zoom_delta(float dDistance) noexcept {
    if (!camera_) {
        return;
    }
    // For FPS-style controls, interpret zoom as FOV change when bound from input
    camera_->orbit_delta(0.0F, 0.0F, dDistance);
}

void VulkanContext::camera_set_aspect(float aspect) noexcept {
    if (!camera_) {
        return;
    }
    if (aspect > 0.0F) {
        camera_->set_aspect(aspect);
    }
}

void VulkanContext::camera_fov_delta(float dFovRadians) noexcept {
    if (!camera_) {
        return;
    }
    camera_->fov_delta(dFovRadians);
}

void VulkanContext::camera_look_delta(float dYaw, float dPitch) noexcept {
    if (!camera_) {
        return;
    }
    camera_->look_delta(dYaw, dPitch);
}

void VulkanContext::camera_move_local(const glm::vec3& deltaLocal) noexcept {
    if (!camera_) {
        return;
    }
    camera_->move_local(deltaLocal);
}

glm::vec3 VulkanContext::camera_position() const noexcept {
    if (!camera_) {
        return glm::vec3 {0.0F, 0.0F, 0.0F};
    }
    return camera_->position();
}

glm::vec2 VulkanContext::camera_angles() const noexcept {
    if (!camera_) {
        return glm::vec2 {0.0F, 0.0F};
    }
    return glm::vec2 {camera_->yaw(), camera_->pitch()};
}

float VulkanContext::camera_fov() const noexcept {
    if (!camera_) {
        return 0.0F;
    }
    return camera_->fov_y();
}

} // namespace vulkano
namespace vulkano {
void VulkanContext::create_blur_render_pass() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (ao_blur_format_ == VK_FORMAT_UNDEFINED) {
        ao_blur_format_ = VK_FORMAT_R8_UNORM;
    }
    VkAttachmentDescription a0 {};
    a0.format = ao_blur_format_;
    a0.samples = VK_SAMPLE_COUNT_1_BIT;
    a0.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    a0.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    a0.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    a0.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    a0.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    a0.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference colorRef {};
    colorRef.attachment = 0U;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1U;
    subpass.pColorAttachments = &colorRef;
    VkSubpassDependency dep {};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0U;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0U;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo rp {};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp.attachmentCount = 1U;
    rp.pAttachments = &a0;
    rp.subpassCount = 1U;
    rp.pSubpasses = &subpass;
    rp.dependencyCount = 1U;
    rp.pDependencies = &dep;
    VkRenderPass pass {VK_NULL_HANDLE};
    if (vkCreateRenderPass(device_, &rp, nullptr, &pass) == VK_SUCCESS) {
        blur_render_pass_ = pass;
        set_object_name(VK_OBJECT_TYPE_RENDER_PASS, reinterpret_cast<std::uint64_t>(blur_render_pass_), "BlurPass");
    }
}

void VulkanContext::destroy_blur_render_pass() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (blur_render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, blur_render_pass_, nullptr);
        blur_render_pass_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::create_blur_targets() noexcept {
    if (device_ == VK_NULL_HANDLE || blur_render_pass_ == VK_NULL_HANDLE) {
        return;
    }
    if (swapchain_extent_.width == 0U || swapchain_extent_.height == 0U) {
        return;
    }
    const std::uint32_t w {swapchain_extent_.width};
    const std::uint32_t h {swapchain_extent_.height};
    create_image_2d(w, h, 1U, ao_blur_format_, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    ao_temp_image_, ao_temp_memory_, "AoTempImage");
    create_image_view_2d(ao_temp_image_, ao_blur_format_, VK_IMAGE_ASPECT_COLOR_BIT, 1U, ao_temp_view_, "AoTempView");
    create_image_2d(w, h, 1U, ao_blur_format_, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    ao_blur_image_, ao_blur_memory_, "AoBlurImage");
    create_image_view_2d(ao_blur_image_, ao_blur_format_, VK_IMAGE_ASPECT_COLOR_BIT, 1U, ao_blur_view_, "AoBlurView");

    // H pass writes aoTemp
    VkImageView attsH[1] {ao_temp_view_};
    VkFramebufferCreateInfo fbH {};
    fbH.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbH.renderPass = blur_render_pass_;
    fbH.attachmentCount = 1U;
    fbH.pAttachments = attsH;
    fbH.width = w; fbH.height = h; fbH.layers = 1U;
    vkCreateFramebuffer(device_, &fbH, nullptr, &blur_h_framebuffer_);
    if (blur_h_framebuffer_ != VK_NULL_HANDLE) {
        set_object_name(VK_OBJECT_TYPE_FRAMEBUFFER, reinterpret_cast<std::uint64_t>(blur_h_framebuffer_), "BlurHFB");
    }
    // V pass writes aoBlur
    VkImageView attsV[1] {ao_blur_view_};
    VkFramebufferCreateInfo fbV {fbH};
    fbV.pAttachments = attsV;
    vkCreateFramebuffer(device_, &fbV, nullptr, &blur_v_framebuffer_);
    if (blur_v_framebuffer_ != VK_NULL_HANDLE) {
        set_object_name(VK_OBJECT_TYPE_FRAMEBUFFER, reinterpret_cast<std::uint64_t>(blur_v_framebuffer_), "BlurVFB");
    }
}

void VulkanContext::destroy_blur_targets() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (blur_h_framebuffer_ != VK_NULL_HANDLE) { vkDestroyFramebuffer(device_, blur_h_framebuffer_, nullptr); blur_h_framebuffer_ = VK_NULL_HANDLE; }
    if (blur_v_framebuffer_ != VK_NULL_HANDLE) { vkDestroyFramebuffer(device_, blur_v_framebuffer_, nullptr); blur_v_framebuffer_ = VK_NULL_HANDLE; }
    if (ao_temp_view_ != VK_NULL_HANDLE) { vkDestroyImageView(device_, ao_temp_view_, nullptr); ao_temp_view_ = VK_NULL_HANDLE; }
    if (ao_temp_image_ != VK_NULL_HANDLE) { vkDestroyImage(device_, ao_temp_image_, nullptr); ao_temp_image_ = VK_NULL_HANDLE; }
    if (ao_temp_memory_ != VK_NULL_HANDLE) { vkFreeMemory(device_, ao_temp_memory_, nullptr); ao_temp_memory_ = VK_NULL_HANDLE; }
    if (ao_blur_view_ != VK_NULL_HANDLE) { vkDestroyImageView(device_, ao_blur_view_, nullptr); ao_blur_view_ = VK_NULL_HANDLE; }
    if (ao_blur_image_ != VK_NULL_HANDLE) { vkDestroyImage(device_, ao_blur_image_, nullptr); ao_blur_image_ = VK_NULL_HANDLE; }
    if (ao_blur_memory_ != VK_NULL_HANDLE) { vkFreeMemory(device_, ao_blur_memory_, nullptr); ao_blur_memory_ = VK_NULL_HANDLE; }
}

void VulkanContext::create_blur_descriptor_set_layout() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    VkDescriptorSetLayoutBinding b0 {};
    b0.binding = 0U;
    b0.descriptorCount = 1U;
    b0.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b0.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutBinding b1 {};
    b1.binding = 1U;
    b1.descriptorCount = 1U;
    b1.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    b1.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutBinding bindings[2] {b0, b1};
    VkDescriptorSetLayoutCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 2U;
    info.pBindings = bindings;
    vkCreateDescriptorSetLayout(device_, &info, nullptr, &blur_set_layout_);
    if (blur_set_layout_ != VK_NULL_HANDLE) {
        set_object_name(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, reinterpret_cast<std::uint64_t>(blur_set_layout_), "BlurSetLayout");
    }
}

void VulkanContext::destroy_blur_descriptor_set_layout() noexcept {
    if (device_ == VK_NULL_HANDLE) { return; }
    if (blur_set_layout_ != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device_, blur_set_layout_, nullptr); blur_set_layout_ = VK_NULL_HANDLE; }
}

void VulkanContext::create_blur_resources() noexcept {
    if (device_ == VK_NULL_HANDLE || blur_set_layout_ == VK_NULL_HANDLE) {
        return;
    }
    const std::uint32_t imageCount {static_cast<std::uint32_t>(swapchain_images_.size())};
    // Uniform buffers per image
    struct BlurParams { glm::vec2 dir; float radius; float sigma; };
    blur_uniform_buffers_.resize(imageCount);
    blur_uniform_memory_.resize(imageCount);
    for (std::uint32_t i {0U}; i < imageCount; ++i) {
        if (blur_uniform_buffers_[i] == VK_NULL_HANDLE) {
            create_buffer(static_cast<VkDeviceSize>(sizeof(BlurParams)), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          blur_uniform_buffers_[i], blur_uniform_memory_[i], "BlurUBO");
        }
        BlurParams p {};
        p.dir = glm::vec2 {1.0F, 0.0F}; // default horizontal
        p.radius = static_cast<float>(ssao_.blurRadius);
        p.sigma = ssao_.blurSigma;
        void* data {nullptr};
        if (vkMapMemory(device_, blur_uniform_memory_[i], 0U, sizeof(BlurParams), 0U, &data) == VK_SUCCESS) {
            std::memcpy(data, &p, sizeof(BlurParams));
            vkUnmapMemory(device_, blur_uniform_memory_[i]);
        }
    }
    // Descriptor pool
    if (blur_descriptor_pool_ == VK_NULL_HANDLE) {
        VkDescriptorPoolSize sizes[2] {};
        sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; sizes[0].descriptorCount = imageCount;
        sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; sizes[1].descriptorCount = imageCount;
        VkDescriptorPoolCreateInfo info {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        info.maxSets = imageCount;
        info.poolSizeCount = 2U;
        info.pPoolSizes = sizes;
        vkCreateDescriptorPool(device_, &info, nullptr, &blur_descriptor_pool_);
    }
    blur_descriptor_sets_.assign(imageCount, VK_NULL_HANDLE);
    if (blur_descriptor_pool_ != VK_NULL_HANDLE) {
        std::vector<VkDescriptorSetLayout> layouts(imageCount, blur_set_layout_);
        VkDescriptorSetAllocateInfo alloc {};
        alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc.descriptorPool = blur_descriptor_pool_;
        alloc.descriptorSetCount = imageCount;
        alloc.pSetLayouts = layouts.data();
        if (vkAllocateDescriptorSets(device_, &alloc, blur_descriptor_sets_.data()) != VK_SUCCESS) {
            return;
        }
        for (std::uint32_t i {0U}; i < imageCount; ++i) {
            VkDescriptorImageInfo aoInfo {};
            aoInfo.sampler = ssao_clamp_sampler_;
            aoInfo.imageView = ao_raw_view_; // initial; command path will swap between H/V
            aoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkDescriptorBufferInfo bufInfo {};
            bufInfo.buffer = blur_uniform_buffers_[i];
            bufInfo.offset = 0U;
            bufInfo.range = sizeof(BlurParams);
            VkWriteDescriptorSet writes[2] {};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = blur_descriptor_sets_[i];
            writes[0].dstBinding = 0U;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[0].descriptorCount = 1U;
            writes[0].pImageInfo = &aoInfo;
            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = blur_descriptor_sets_[i];
            writes[1].dstBinding = 1U;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[1].descriptorCount = 1U;
            writes[1].pBufferInfo = &bufInfo;
            vkUpdateDescriptorSets(device_, 2U, writes, 0U, nullptr);
        }
    }
}

void VulkanContext::destroy_blur_resources() noexcept {
    if (device_ == VK_NULL_HANDLE) { return; }
    if (blur_descriptor_pool_ != VK_NULL_HANDLE) { vkDestroyDescriptorPool(device_, blur_descriptor_pool_, nullptr); blur_descriptor_pool_ = VK_NULL_HANDLE; }
    blur_descriptor_sets_.clear();
    for (auto& b : blur_uniform_buffers_) { if (b != VK_NULL_HANDLE) { vkDestroyBuffer(device_, b, nullptr); b = VK_NULL_HANDLE; } }
    for (auto& m : blur_uniform_memory_) { if (m != VK_NULL_HANDLE) { vkFreeMemory(device_, m, nullptr); m = VK_NULL_HANDLE; } }
}

void VulkanContext::create_blur_pipeline() noexcept {
    if (device_ == VK_NULL_HANDLE || blur_render_pass_ == VK_NULL_HANDLE || blur_set_layout_ == VK_NULL_HANDLE) {
        return;
    }
    VkPipelineLayoutCreateInfo pl {};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount = 1U;
    const VkDescriptorSetLayout sets[1] {blur_set_layout_};
    pl.pSetLayouts = sets;
    if (vkCreatePipelineLayout(device_, &pl, nullptr, &blur_pipeline_layout_) != VK_SUCCESS) {
        return;
    }
    set_object_name(VK_OBJECT_TYPE_PIPELINE_LAYOUT, reinterpret_cast<std::uint64_t>(blur_pipeline_layout_), "BlurPipelineLayout");

    const std::string dir {shader_dir_guess()};
    const std::vector<char> vertCode {read_file_binary(dir + "/ssao.vert.spv")};
    const std::vector<char> fragCode {read_file_binary(dir + "/blur.frag.spv")};
    if (vertCode.empty() || fragCode.empty()) { return; }
    const VkShaderModule vertModule {create_shader_module(device_, vertCode)};
    const VkShaderModule fragModule {create_shader_module(device_, fragCode)};
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
        if (vertModule != VK_NULL_HANDLE) { vkDestroyShaderModule(device_, vertModule, nullptr); }
        if (fragModule != VK_NULL_HANDLE) { vkDestroyShaderModule(device_, fragModule, nullptr); }
        return;
    }
    VkPipelineShaderStageCreateInfo vs {};
    vs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; vs.stage = VK_SHADER_STAGE_VERTEX_BIT; vs.module = vertModule; vs.pName = "main";
    VkPipelineShaderStageCreateInfo fs {};
    fs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; fs.stage = VK_SHADER_STAGE_FRAGMENT_BIT; fs.module = fragModule; fs.pName = "main";
    const VkPipelineShaderStageCreateInfo stages[2] {vs, fs};
    VkPipelineVertexInputStateCreateInfo vi { };
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkPipelineInputAssemblyStateCreateInfo ia { };
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO; ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport vp { };
    vp.x = 0.0F; vp.y = 0.0F; vp.width = static_cast<float>(swapchain_extent_.width); vp.height = static_cast<float>(swapchain_extent_.height); vp.minDepth = 0.0F; vp.maxDepth = 1.0F;
    VkRect2D sc { }; sc.offset = {0,0}; sc.extent = swapchain_extent_;
    VkPipelineViewportStateCreateInfo vpst { }; vpst.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO; vpst.viewportCount = 1U; vpst.pViewports = &vp; vpst.scissorCount = 1U; vpst.pScissors = &sc;
    VkPipelineRasterizationStateCreateInfo rs { }; rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO; rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_NONE; rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.lineWidth = 1.0F;
    VkPipelineMultisampleStateCreateInfo ms { }; ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO; ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo ds { }; ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO; ds.depthTestEnable = VK_FALSE; ds.depthWriteEnable = VK_FALSE;
    VkPipelineColorBlendAttachmentState att { }; att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT; att.blendEnable = VK_FALSE;
    VkPipelineColorBlendStateCreateInfo cb { }; cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO; cb.attachmentCount = 1U; cb.pAttachments = &att;
    VkGraphicsPipelineCreateInfo gp { };
    gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO; gp.stageCount = 2U; gp.pStages = stages; gp.pVertexInputState = &vi; gp.pInputAssemblyState = &ia; gp.pViewportState = &vpst; gp.pRasterizationState = &rs; gp.pMultisampleState = &ms; gp.pDepthStencilState = &ds; gp.pColorBlendState = &cb; gp.layout = blur_pipeline_layout_; gp.renderPass = blur_render_pass_; gp.subpass = 0U;
    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1U, &gp, nullptr, &blur_pipeline_) == VK_SUCCESS) {
        set_object_name(VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<std::uint64_t>(blur_pipeline_), "BlurPipeline");
    }
    vkDestroyShaderModule(device_, vertModule, nullptr);
    vkDestroyShaderModule(device_, fragModule, nullptr);
}

void VulkanContext::destroy_blur_pipeline() noexcept {
    if (device_ == VK_NULL_HANDLE) { return; }
    if (blur_pipeline_ != VK_NULL_HANDLE) { vkDestroyPipeline(device_, blur_pipeline_, nullptr); blur_pipeline_ = VK_NULL_HANDLE; }
    if (blur_pipeline_layout_ != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device_, blur_pipeline_layout_, nullptr); blur_pipeline_layout_ = VK_NULL_HANDLE; }
}

void VulkanContext::create_compose_descriptor_set_layout() noexcept {
    if (device_ == VK_NULL_HANDLE) { return; }
    // set=4: litTex, aoTex, ComposeParams UBO
    VkDescriptorSetLayoutBinding b0 {};
    b0.binding = 0U; b0.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; b0.descriptorCount = 1U; b0.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutBinding b1 {b0}; b1.binding = 1U;
    VkDescriptorSetLayoutBinding b2 {};
    b2.binding = 2U; b2.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; b2.descriptorCount = 1U; b2.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutBinding bindings[3] {b0, b1, b2};
    VkDescriptorSetLayoutCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; info.bindingCount = 3U; info.pBindings = bindings;
    vkCreateDescriptorSetLayout(device_, &info, nullptr, &compose_set_layout_);
    if (compose_set_layout_ != VK_NULL_HANDLE) {
        set_object_name(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, reinterpret_cast<std::uint64_t>(compose_set_layout_), "ComposeSetLayout");
    }
}

void VulkanContext::destroy_compose_descriptor_set_layout() noexcept {
    if (device_ == VK_NULL_HANDLE) { return; }
    if (compose_set_layout_ != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device_, compose_set_layout_, nullptr); compose_set_layout_ = VK_NULL_HANDLE; }
}

void VulkanContext::create_compose_pipeline() noexcept {
    if (device_ == VK_NULL_HANDLE || render_pass_ == VK_NULL_HANDLE || compose_set_layout_ == VK_NULL_HANDLE) {
        return;
    }
    VkPipelineLayoutCreateInfo pl {};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount = 1U;
    const VkDescriptorSetLayout setLayouts[1] {compose_set_layout_};
    pl.pSetLayouts = setLayouts;
    if (vkCreatePipelineLayout(device_, &pl, nullptr, &compose_pipeline_layout_) != VK_SUCCESS) {
        return;
    }
    set_object_name(VK_OBJECT_TYPE_PIPELINE_LAYOUT, reinterpret_cast<std::uint64_t>(compose_pipeline_layout_), "ComposePipelineLayout");

    const std::string dir {shader_dir_guess()};
    const std::vector<char> vertCode {read_file_binary(dir + "/ssao.vert.spv")};
    const std::vector<char> fragCode {read_file_binary(dir + "/compose.frag.spv")};
    if (vertCode.empty() || fragCode.empty()) { return; }
    const VkShaderModule vertModule {create_shader_module(device_, vertCode)};
    const VkShaderModule fragModule {create_shader_module(device_, fragCode)};
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
        if (vertModule != VK_NULL_HANDLE) { vkDestroyShaderModule(device_, vertModule, nullptr); }
        if (fragModule != VK_NULL_HANDLE) { vkDestroyShaderModule(device_, fragModule, nullptr); }
        return;
    }
    VkPipelineShaderStageCreateInfo vs {};
    vs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; vs.stage = VK_SHADER_STAGE_VERTEX_BIT; vs.module = vertModule; vs.pName = "main";
    VkPipelineShaderStageCreateInfo fs {};
    fs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; fs.stage = VK_SHADER_STAGE_FRAGMENT_BIT; fs.module = fragModule; fs.pName = "main";
    const VkPipelineShaderStageCreateInfo stages[2] {vs, fs};
    VkPipelineVertexInputStateCreateInfo vi { }; vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkPipelineInputAssemblyStateCreateInfo ia { }; ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO; ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport vp { }; vp.x = 0.0F; vp.y = 0.0F; vp.width = static_cast<float>(swapchain_extent_.width); vp.height = static_cast<float>(swapchain_extent_.height); vp.minDepth = 0.0F; vp.maxDepth = 1.0F;
    VkRect2D sc { }; sc.offset = {0,0}; sc.extent = swapchain_extent_;
    VkPipelineViewportStateCreateInfo vpst { }; vpst.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO; vpst.viewportCount = 1U; vpst.pViewports = &vp; vpst.scissorCount = 1U; vpst.pScissors = &sc;
    VkPipelineRasterizationStateCreateInfo rs { }; rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO; rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_NONE; rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.lineWidth = 1.0F;
    VkPipelineMultisampleStateCreateInfo ms { }; ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO; ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo ds { }; ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO; ds.depthTestEnable = VK_FALSE; ds.depthWriteEnable = VK_FALSE;
    VkPipelineColorBlendAttachmentState att { }; att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT; att.blendEnable = VK_FALSE;
    VkPipelineColorBlendStateCreateInfo cb { }; cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO; cb.attachmentCount = 1U; cb.pAttachments = &att;
    VkGraphicsPipelineCreateInfo gp { };
    gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO; gp.stageCount = 2U; gp.pStages = stages; gp.pVertexInputState = &vi; gp.pInputAssemblyState = &ia; gp.pViewportState = &vpst; gp.pRasterizationState = &rs; gp.pMultisampleState = &ms; gp.pDepthStencilState = &ds; gp.pColorBlendState = &cb; gp.layout = compose_pipeline_layout_; gp.renderPass = render_pass_; gp.subpass = 0U;
    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1U, &gp, nullptr, &compose_pipeline_) == VK_SUCCESS) {
        set_object_name(VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<std::uint64_t>(compose_pipeline_), "ComposePipeline");
    }
    vkDestroyShaderModule(device_, vertModule, nullptr);
    vkDestroyShaderModule(device_, fragModule, nullptr);
}

void VulkanContext::destroy_compose_pipeline() noexcept {
    if (device_ == VK_NULL_HANDLE) { return; }
    if (compose_pipeline_ != VK_NULL_HANDLE) { vkDestroyPipeline(device_, compose_pipeline_, nullptr); compose_pipeline_ = VK_NULL_HANDLE; }
    if (compose_pipeline_layout_ != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device_, compose_pipeline_layout_, nullptr); compose_pipeline_layout_ = VK_NULL_HANDLE; }
}
} // namespace vulkano
