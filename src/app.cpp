#include <vulkano_codex/app.hpp>

#include <array>
#include <cassert>
#include <cstring>
#include <stdexcept>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <imgui.h>

#include <vulkano_codex/imgui_layer.hpp>
#include <vulkano_codex/utils.hpp>

namespace vulkano_codex {

namespace {
    const std::array<const char*, 1> device_extensions {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifndef NDEBUG
    constexpr bool enable_validation_layers {true};
    const std::array<const char*, 1> validation_layers {"VK_LAYER_KHRONOS_validation"};
#else
    constexpr bool enable_validation_layers {false};
    const std::array<const char*, 0> validation_layers {};
#endif

    VkResult create_debug_utils_messenger_ext(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                              const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger) {
        const auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        if (func != nullptr) {
            return func(instance, pCreateInfo, pAllocator, pMessenger);
        } else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void destroy_debug_utils_messenger_ext(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator) noexcept {
        const auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (func != nullptr) {
            func(instance, messenger, pAllocator);
        }
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                  VkDebugUtilsMessageTypeFlagsEXT /*type*/, const VkDebugUtilsMessengerCallbackDataEXT* data,
                                                  void* /*user*/) {
        if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            fprintf(stderr, "Validation: %s\n", data->pMessage);
        }
        return VK_FALSE;
    }

    struct Vertex final {
        glm::vec2 pos {};
    };

    constexpr std::array<Vertex, 3> triangle_vertices {
        Vertex {glm::vec2 {-0.5f, -0.5f}},
        Vertex {glm::vec2 {0.5f, -0.5f}},
        Vertex {glm::vec2 {0.0f, 0.5f}},
    };

    void set_object_name(VkDevice device, VkObjectType type, uint64_t handle, const char* name) {
        static auto func = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
        if (!enable_validation_layers || func == nullptr) {
            return;
        }
        VkDebugUtilsObjectNameInfoEXT info {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        info.objectType = type;
        info.objectHandle = handle;
        info.pObjectName = name;
        func(device, &info);
    }

    void cmd_begin_label(VkDevice device, VkCommandBuffer cmd, const char* label, float r, float g, float b, float a) {
        static auto func = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT"));
        if (!enable_validation_layers || func == nullptr) {
            return;
        }
        VkDebugUtilsLabelEXT info {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
        info.pLabelName = label;
        info.color[0] = r;
        info.color[1] = g;
        info.color[2] = b;
        info.color[3] = a;
        func(cmd, &info);
    }

    void cmd_end_label(VkDevice device, VkCommandBuffer cmd) {
        static auto func = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT"));
        if (!enable_validation_layers || func == nullptr) {
            return;
        }
        func(cmd);
    }
}

App::~App() {
    vkDeviceWaitIdle(device_);
    if (imgui_ != nullptr) {
        imgui_->shutdown(device_);
        delete imgui_;
        imgui_ = nullptr;
    }
    cleanup_swapchain();
    if (vertex_buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, vertex_buffer_, nullptr);
        vertex_buffer_ = VK_NULL_HANDLE;
    }
    if (vertex_buffer_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, vertex_buffer_memory_, nullptr);
        vertex_buffer_memory_ = VK_NULL_HANDLE;
    }
    if (command_pool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, command_pool_, nullptr);
        command_pool_ = VK_NULL_HANDLE;
    }
    for (size_t i = 0; i < max_frames_in_flight_; ++i) {
        if (image_available_[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, image_available_[i], nullptr);
        }
        if (render_finished_[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, render_finished_[i], nullptr);
        }
        if (in_flight_fences_[i] != VK_NULL_HANDLE) {
            vkDestroyFence(device_, in_flight_fences_[i], nullptr);
        }
    }
    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    if (debug_messenger_ != VK_NULL_HANDLE) {
        destroy_debug_utils_messenger_ext(instance_, debug_messenger_, nullptr);
        debug_messenger_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
    if (window_ != nullptr) {
        glfwDestroyWindow(static_cast<GLFWwindow*>(window_));
        window_ = nullptr;
    }
    glfwTerminate();
}

void App::run() {
    init_window();
    init_vulkan();

    FpsAverager fps {};
    fps.reset();

    while (!glfwWindowShouldClose(static_cast<GLFWwindow*>(window_))) {
        glfwPollEvents();
        fps.tick();

        // Start ImGui frame and build overlay
        imgui_->begin_frame();
        ImGui::Begin("Stats");
        ImGui::Text("FPS: %.1f", fps.fps());
        ImGui::Text("Frame time: %.2f ms", fps.frame_ms());
        ImGui::Text("Device: %s", device_name_.c_str());
        ImGui::Text("Extent: %u x %u", swapchain_extent_.width, swapchain_extent_.height);
        ImGui::End();

        draw_frame();
    }
    vkDeviceWaitIdle(device_);
}

void App::init_window() {
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("Failed to init GLFW");
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    window_ = glfwCreateWindow(initial_width_, initial_height_, app_name_, nullptr, nullptr);
    if (window_ == nullptr) {
        throw std::runtime_error("Failed to create window");
    }
    glfwSetWindowUserPointer(static_cast<GLFWwindow*>(window_), this);
    glfwSetFramebufferSizeCallback(static_cast<GLFWwindow*>(window_), [](GLFWwindow* win, int /*w*/, int /*h*/) {
        auto* app = static_cast<App*>(glfwGetWindowUserPointer(win));
        app->framebuffer_resized_ = true;
    });
}

void App::init_vulkan() {
    create_instance();
    setup_debug_messenger();
    create_surface();
    pick_physical_device();
    create_logical_device();
    create_swapchain();
    create_image_views();
    create_render_pass();
    create_pipeline();
    create_framebuffers();
    create_command_pool();
    create_vertex_buffer();
    create_command_buffers();
    create_sync_objects();
    init_imgui();
}

void App::create_instance() {
    if (enable_validation_layers) {
        uint32_t count = 0;
        vkEnumerateInstanceLayerProperties(&count, nullptr);
        std::vector<VkLayerProperties> props(count);
        vkEnumerateInstanceLayerProperties(&count, props.data());
        for (const char* layer : validation_layers) {
            bool found = false;
            for (const auto& p : props) {
                if (strcmp(p.layerName, layer) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw std::runtime_error("Validation layer not found");
            }
        }
    }

    VkApplicationInfo app_info {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app_info.pApplicationName = app_name_;
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "NoEngine";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    uint32_t glfw_ext_count = 0U;
    const char** glfw_ext = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
    std::vector<const char*> extensions(glfw_ext, glfw_ext + glfw_ext_count);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

#ifdef __APPLE__
    // Portability enumeration for MoltenVK
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

    VkInstanceCreateInfo create_info {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledLayerCount = enable_validation_layers ? static_cast<uint32_t>(validation_layers.size()) : 0U;
    create_info.ppEnabledLayerNames = enable_validation_layers ? validation_layers.data() : nullptr;
#ifdef __APPLE__
    create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    VkDebugUtilsMessengerCreateInfoEXT dbg_info {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    if (enable_validation_layers) {
        dbg_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbg_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbg_info.pfnUserCallback = debug_callback;
        create_info.pNext = &dbg_info;
    }

    if (vkCreateInstance(&create_info, nullptr, &instance_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create instance");
    }
}

void App::setup_debug_messenger() {
    if (!enable_validation_layers) {
        return;
    }
    VkDebugUtilsMessengerCreateInfoEXT info {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = debug_callback;
    if (create_debug_utils_messenger_ext(instance_, &info, nullptr, &debug_messenger_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create debug messenger");
    }
}

void App::create_surface() {
    if (glfwCreateWindowSurface(instance_, static_cast<GLFWwindow*>(window_), nullptr, &surface_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create surface");
    }
}

QueueFamilyIndices App::find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices {};
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> props(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, props.data());
    for (uint32_t i = 0; i < count; ++i) {
        if ((props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
            indices.graphics = i;
        }
        VkBool32 present_supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_supported);
        if (present_supported == VK_TRUE) {
            indices.present = i;
        }
        if (indices.complete()) {
            break;
        }
    }
    return indices;
}

void App::pick_physical_device() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) {
        throw std::runtime_error("No Vulkan physical devices found");
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    VkPhysicalDevice selected {VK_NULL_HANDLE};
    for (const auto dev : devices) {
        QueueFamilyIndices indices = find_queue_families(dev, surface_);
        if (!indices.complete()) {
            continue;
        }
        // Check required extensions
        uint32_t ext_count = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &ext_count, nullptr);
        std::vector<VkExtensionProperties> exts(ext_count);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &ext_count, exts.data());
        bool has_swapchain = false;
        for (const auto& e : exts) {
            if (strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                has_swapchain = true;
                break;
            }
        }
        if (!has_swapchain) {
            continue;
        }
        selected = dev;
        queue_indices_ = indices;
        break;
    }
    if (selected == VK_NULL_HANDLE) {
        throw std::runtime_error("No suitable physical device found");
    }
    physical_device_ = selected;

    VkPhysicalDeviceProperties props {};
    vkGetPhysicalDeviceProperties(physical_device_, &props);
    device_name_ = props.deviceName;
}

void App::create_logical_device() {
    std::vector<VkDeviceQueueCreateInfo> queue_infos {};
    const uint32_t g_family = queue_indices_.graphics.value();
    const uint32_t p_family = queue_indices_.present.value();
    const float priority {1.0f};
    VkDeviceQueueCreateInfo qinfo_g {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qinfo_g.queueFamilyIndex = g_family;
    qinfo_g.queueCount = 1;
    qinfo_g.pQueuePriorities = &priority;
    queue_infos.push_back(qinfo_g);
    if (p_family != g_family) {
        VkDeviceQueueCreateInfo qinfo_p {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qinfo_p.queueFamilyIndex = p_family;
        qinfo_p.queueCount = 1;
        qinfo_p.pQueuePriorities = &priority;
        queue_infos.push_back(qinfo_p);
    }

    VkPhysicalDeviceFeatures features {};

    std::vector<const char*> exts {};
    exts.insert(exts.end(), device_extensions.begin(), device_extensions.end());
#ifdef __APPLE__
    exts.push_back("VK_KHR_portability_subset");
#endif

    VkDeviceCreateInfo info {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    info.queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size());
    info.pQueueCreateInfos = queue_infos.data();
    info.pEnabledFeatures = &features;
    info.enabledExtensionCount = static_cast<uint32_t>(exts.size());
    info.ppEnabledExtensionNames = exts.data();
    info.enabledLayerCount = enable_validation_layers ? static_cast<uint32_t>(validation_layers.size()) : 0U;
    info.ppEnabledLayerNames = enable_validation_layers ? validation_layers.data() : nullptr;

    if (vkCreateDevice(physical_device_, &info, nullptr, &device_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }
    vkGetDeviceQueue(device_, g_family, 0, &graphics_queue_);
    vkGetDeviceQueue(device_, p_family, 0, &present_queue_);
    set_object_name(device_, VK_OBJECT_TYPE_DEVICE, reinterpret_cast<uint64_t>(device_), "LogicalDevice");
}

void App::create_swapchain() {
    VkSurfaceCapabilitiesKHR caps {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &caps);

    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, formats.data());
    VkSurfaceFormatKHR chosen_format = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen_format = f;
            break;
        }
    }
    swapchain_format_ = chosen_format.format;

    VkExtent2D extent {};
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        extent = caps.currentExtent;
    } else {
        int w = 0;
        int h = 0;
        glfwGetFramebufferSize(static_cast<GLFWwindow*>(window_), &w, &h);
        extent.width = static_cast<uint32_t>(w);
        extent.height = static_cast<uint32_t>(h);
        extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
        extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    swapchain_extent_ = extent;

    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
        image_count = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR info {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    info.surface = surface_;
    info.minImageCount = image_count;
    info.imageFormat = swapchain_format_;
    info.imageColorSpace = chosen_format.colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const uint32_t g_family = queue_indices_.graphics.value();
    const uint32_t p_family = queue_indices_.present.value();
    if (g_family != p_family) {
        const uint32_t indices[2] {g_family, p_family};
        info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices = indices;
    } else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    info.preTransform = caps.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    info.clipped = VK_TRUE;
    info.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device_, &info, nullptr, &swapchain_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain");
    }
    set_object_name(device_, VK_OBJECT_TYPE_SWAPCHAIN_KHR, reinterpret_cast<uint64_t>(swapchain_), "Swapchain");
    uint32_t actual_count = 0;
    vkGetSwapchainImagesKHR(device_, swapchain_, &actual_count, nullptr);
    swapchain_images_.resize(actual_count);
    vkGetSwapchainImagesKHR(device_, swapchain_, &actual_count, swapchain_images_.data());
}

void App::create_image_views() {
    swapchain_image_views_.resize(swapchain_images_.size());
    for (size_t i = 0; i < swapchain_images_.size(); ++i) {
        VkImageViewCreateInfo info {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        info.image = swapchain_images_[i];
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = swapchain_format_;
        info.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device_, &info, nullptr, &swapchain_image_views_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view");
        }
        set_object_name(device_, VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64_t>(swapchain_image_views_[i]), "SwapchainImageView");
    }
}

void App::create_render_pass() {
    VkAttachmentDescription color {};
    color.format = swapchain_format_;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_ref {};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

    VkSubpassDependency dep {};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    info.attachmentCount = 1;
    info.pAttachments = &color;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dep;
    if (vkCreateRenderPass(device_, &info, nullptr, &render_pass_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }
    set_object_name(device_, VK_OBJECT_TYPE_RENDER_PASS, reinterpret_cast<uint64_t>(render_pass_), "RenderPass");
}

static std::vector<char> read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f == nullptr) {
        throw std::runtime_error("Failed to open shader file");
    }
    fseek(f, 0, SEEK_END);
    const long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<char> data(static_cast<size_t>(sz));
    fread(data.data(), 1, static_cast<size_t>(sz), f);
    fclose(f);
    return data;
}

void App::create_pipeline() {
    const std::string vert_path = std::string {SHADER_DIR} + "/triangle.vert.spv";
    const std::string frag_path = std::string {SHADER_DIR} + "/triangle.frag.spv";
    const auto vert_code = read_file(vert_path.c_str());
    const auto frag_code = read_file(frag_path.c_str());

    VkShaderModuleCreateInfo sm_info {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    sm_info.codeSize = vert_code.size();
    sm_info.pCode = reinterpret_cast<const uint32_t*>(vert_code.data());
    VkShaderModule vert_module {VK_NULL_HANDLE};
    if (vkCreateShaderModule(device_, &sm_info, nullptr, &vert_module) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vertex shader module");
    }
    sm_info.codeSize = frag_code.size();
    sm_info.pCode = reinterpret_cast<const uint32_t*>(frag_code.data());
    VkShaderModule frag_module {VK_NULL_HANDLE};
    if (vkCreateShaderModule(device_, &sm_info, nullptr, &frag_module) != VK_SUCCESS) {
        vkDestroyShaderModule(device_, vert_module, nullptr);
        throw std::runtime_error("Failed to create fragment shader module");
    }

    VkPipelineShaderStageCreateInfo vs {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    vs.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vs.module = vert_module;
    vs.pName = "main";
    VkPipelineShaderStageCreateInfo fs {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    fs.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fs.module = frag_module;
    fs.pName = "main";
    VkPipelineShaderStageCreateInfo stages[] {vs, fs};

    std::array<VkVertexInputBindingDescription, 1> bindings {VkVertexInputBindingDescription {0, sizeof(glm::vec2), VK_VERTEX_INPUT_RATE_VERTEX}};
    std::array<VkVertexInputAttributeDescription, 1> attrs {VkVertexInputAttributeDescription {0, 0, VK_FORMAT_R32G32_SFLOAT, 0}};
    VkPipelineVertexInputStateCreateInfo vi {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
    vi.pVertexBindingDescriptions = bindings.data();
    vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vi.pVertexAttributeDescriptions = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo ia {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ia.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain_extent_.width);
    viewport.height = static_cast<float>(swapchain_extent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor {{0, 0}, swapchain_extent_};
    VkPipelineViewportStateCreateInfo vp {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.pViewports = &viewport;
    vp.scissorCount = 1;
    vp.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rs {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.depthClampEnable = VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.depthBiasEnable = VK_FALSE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cb_att {};
    cb_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cb_att.blendEnable = VK_FALSE;
    VkPipelineColorBlendStateCreateInfo cb {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &cb_att;

    VkPushConstantRange push {};
    push.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    push.offset = 0;
    push.size = sizeof(float) * 4;

    VkPipelineLayoutCreateInfo pl {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &push;
    if (vkCreatePipelineLayout(device_, &pl, nullptr, &pipeline_layout_) != VK_SUCCESS) {
        vkDestroyShaderModule(device_, vert_module, nullptr);
        vkDestroyShaderModule(device_, frag_module, nullptr);
        throw std::runtime_error("Failed to create pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pi {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pi.stageCount = 2;
    pi.pStages = stages;
    pi.pVertexInputState = &vi;
    pi.pInputAssemblyState = &ia;
    pi.pViewportState = &vp;
    pi.pRasterizationState = &rs;
    pi.pMultisampleState = &ms;
    pi.pColorBlendState = &cb;
    pi.layout = pipeline_layout_;
    pi.renderPass = render_pass_;
    pi.subpass = 0;

    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pi, nullptr, &pipeline_) != VK_SUCCESS) {
        vkDestroyShaderModule(device_, vert_module, nullptr);
        vkDestroyShaderModule(device_, frag_module, nullptr);
        throw std::runtime_error("Failed to create graphics pipeline");
    }
    set_object_name(device_, VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<uint64_t>(pipeline_), "GraphicsPipeline");

    vkDestroyShaderModule(device_, vert_module, nullptr);
    vkDestroyShaderModule(device_, frag_module, nullptr);
}

void App::create_framebuffers() {
    framebuffers_.resize(swapchain_image_views_.size());
    for (size_t i = 0; i < swapchain_image_views_.size(); ++i) {
        VkImageView attachments[] {swapchain_image_views_[i]};
        VkFramebufferCreateInfo info {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        info.renderPass = render_pass_;
        info.attachmentCount = 1;
        info.pAttachments = attachments;
        info.width = swapchain_extent_.width;
        info.height = swapchain_extent_.height;
        info.layers = 1;
        if (vkCreateFramebuffer(device_, &info, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}

void App::create_command_pool() {
    VkCommandPoolCreateInfo info {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    info.queueFamilyIndex = queue_indices_.graphics.value();
    info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(device_, &info, nullptr, &command_pool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }
    set_object_name(device_, VK_OBJECT_TYPE_COMMAND_POOL, reinterpret_cast<uint64_t>(command_pool_), "CmdPool");
}

uint32_t App::find_memory_type(VkPhysicalDevice physical, uint32_t type_filter, VkMemoryPropertyFlags props) noexcept {
    VkPhysicalDeviceMemoryProperties mem {};
    vkGetPhysicalDeviceMemoryProperties(physical, &mem);
    for (uint32_t i = 0; i < mem.memoryTypeCount; ++i) {
        if ((type_filter & (1u << i)) != 0U && (mem.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    return 0;
}

void App::create_vertex_buffer() {
    VkDeviceSize size = static_cast<VkDeviceSize>(triangle_vertices.size() * sizeof(triangle_vertices[0]));
    VkBufferCreateInfo info {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size;
    info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &info, nullptr, &vertex_buffer_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vertex buffer");
    }
    set_object_name(device_, VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(vertex_buffer_), "VertexBuffer");
    VkMemoryRequirements req {};
    vkGetBufferMemoryRequirements(device_, vertex_buffer_, &req);
    VkMemoryAllocateInfo alloc {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = find_memory_type(physical_device_, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(device_, &alloc, nullptr, &vertex_buffer_memory_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate vertex buffer memory");
    }
    vkBindBufferMemory(device_, vertex_buffer_, vertex_buffer_memory_, 0);
    void* data = nullptr;
    vkMapMemory(device_, vertex_buffer_memory_, 0, size, 0, &data);
    std::memcpy(data, triangle_vertices.data(), static_cast<size_t>(size));
    vkUnmapMemory(device_, vertex_buffer_memory_);
}

void App::create_command_buffers() {
    command_buffers_.resize(framebuffers_.size());
    VkCommandBufferAllocateInfo alloc {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc.commandPool = command_pool_;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = static_cast<uint32_t>(command_buffers_.size());
    if (vkAllocateCommandBuffers(device_, &alloc, command_buffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }
}

void App::create_sync_objects() {
    VkSemaphoreCreateInfo sem {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fence {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (size_t i = 0; i < max_frames_in_flight_; ++i) {
        if (vkCreateSemaphore(device_, &sem, nullptr, &image_available_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device_, &sem, nullptr, &render_finished_[i]) != VK_SUCCESS ||
            vkCreateFence(device_, &fence, nullptr, &in_flight_fences_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create sync objects");
        }
    }
}

void App::init_imgui() {
    imgui_ = new ImGuiLayer();
    imgui_->init(instance_, physical_device_, device_, queue_indices_.graphics.value(), graphics_queue_, render_pass_, static_cast<uint32_t>(swapchain_images_.size()), window_);
}

void App::recreate_swapchain() {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(static_cast<GLFWwindow*>(window_), &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(static_cast<GLFWwindow*>(window_), &width, &height);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(device_);
    cleanup_swapchain();
    create_swapchain();
    create_image_views();
    create_render_pass();
    create_pipeline();
    create_framebuffers();
    create_command_buffers();
    if (imgui_ != nullptr) {
        imgui_->shutdown(device_);
        delete imgui_;
        imgui_ = nullptr;
    }
    init_imgui();
}

void App::cleanup_swapchain() {
    for (auto fb : framebuffers_) {
        vkDestroyFramebuffer(device_, fb, nullptr);
    }
    framebuffers_.clear();
    if (!command_buffers_.empty()) {
        vkFreeCommandBuffers(device_, command_pool_, static_cast<uint32_t>(command_buffers_.size()), command_buffers_.data());
        command_buffers_.clear();
    }
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (pipeline_layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
        pipeline_layout_ = VK_NULL_HANDLE;
    }
    if (render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, render_pass_, nullptr);
        render_pass_ = VK_NULL_HANDLE;
    }
    for (auto view : swapchain_image_views_) {
        vkDestroyImageView(device_, view, nullptr);
    }
    swapchain_image_views_.clear();
    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

void App::draw_frame() {
    static constexpr uint64_t fence_timeout_ns {1000000000ull};
    vkWaitForFences(device_, 1, &in_flight_fences_[current_frame_], VK_TRUE, fence_timeout_ns);
    vkResetFences(device_, 1, &in_flight_fences_[current_frame_]);

    uint32_t image_index = 0;
    const VkResult acq = vkAcquireNextImageKHR(device_, swapchain_, fence_timeout_ns, image_available_[current_frame_], VK_NULL_HANDLE, &image_index);
    if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return;
    }

    VkCommandBuffer cmd = command_buffers_[image_index];
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo begin {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cmd, &begin);

    VkClearValue clear {{0.0f, 0.0f, 0.0f, 1.0f}};
    VkRenderPassBeginInfo rp {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp.renderPass = render_pass_;
    rp.framebuffer = framebuffers_[image_index];
    rp.renderArea = {{0, 0}, swapchain_extent_};
    rp.clearValueCount = 1;
    rp.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    VkDeviceSize offsets[] {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer_, offsets);
    cmd_begin_label(device_, cmd, "DrawTriangle", 0.9f, 0.9f, 0.2f, 1.0f);
    const float white[4] {1.0f, 1.0f, 1.0f, 1.0f};
    vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(white), white);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    cmd_end_label(device_, cmd);

    // ImGui overlay render
    imgui_->end_frame(cmd);

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkSemaphore wait_sems[] {image_available_[current_frame_]};
    VkPipelineStageFlags wait_stages[] {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signal_sems[] {render_finished_[current_frame_]};
    VkSubmitInfo submit {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = wait_sems;
    submit.pWaitDstStageMask = wait_stages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = signal_sems;
    if (vkQueueSubmit(graphics_queue_, 1, &submit, in_flight_fences_[current_frame_]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw");
    }

    VkPresentInfoKHR present {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = signal_sems;
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain_;
    present.pImageIndices = &image_index;
    const VkResult p = vkQueuePresentKHR(present_queue_, &present);
    if (p == VK_ERROR_OUT_OF_DATE_KHR || p == VK_SUBOPTIMAL_KHR || framebuffer_resized_) {
        framebuffer_resized_ = false;
        recreate_swapchain();
    } else if (p != VK_SUCCESS) {
        throw std::runtime_error("Failed to present");
    }
    current_frame_ = (current_frame_ + 1) % max_frames_in_flight_;
}

} // namespace vulkano_codex
