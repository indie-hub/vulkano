// vulkan_context.cpp
#include <vulkano/vulkan_context.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

namespace shaders {
std::vector<std::uint32_t> vertex_spirv();
std::vector<std::uint32_t> fragment_spirv();
}

namespace vulkano {

namespace {
    constexpr std::uint32_t kMaxFramesInFlight {2U};
    constexpr std::array<const char*, 1> kDeviceExtensions {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#ifndef NDEBUG
    constexpr bool kEnableValidation {true};
#else
    constexpr bool kEnableValidation {false};
#endif

    VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                  VkDebugUtilsMessageTypeFlagsEXT type,
                                                  const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
                                                  void* user_data) {
        (void)type;
        (void)user_data;
        if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            fprintf(stderr, "[Vulkan] %s\n", callback_data->pMessage);
        }
        return VK_FALSE;
    }

    VkShaderModule create_shader_module(VkDevice device, const std::vector<std::uint32_t>& code) {
        VkShaderModuleCreateInfo info {};
        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = code.size() * sizeof(std::uint32_t);
        info.pCode = code.data();
        VkShaderModule module {nullptr};
        if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS) {
            throw std::runtime_error{"Failed to create shader module"};
        }
        return module;
    }

    struct QueueFamilies {
        std::uint32_t graphics {UINT32_MAX};
        std::uint32_t present {UINT32_MAX};
        [[nodiscard]] bool complete() const noexcept { return graphics != UINT32_MAX && present != UINT32_MAX; }
    };

    QueueFamilies find_queue_families(VkPhysicalDevice pd, VkSurfaceKHR surface) {
        std::uint32_t count {0U};
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &count, nullptr);
        std::vector<VkQueueFamilyProperties> props(count);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &count, props.data());
        QueueFamilies q {};
        for (std::uint32_t i = 0; i < count; ++i) {
            if ((props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
                q.graphics = i;
            }
            VkBool32 present_support {VK_FALSE};
            vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, surface, &present_support);
            if (present_support == VK_TRUE) {
                q.present = i;
            }
            if (q.complete()) {
                break;
            }
        }
        return q;
    }

    struct SwapSupport {
        VkSurfaceCapabilitiesKHR caps {};
        std::vector<VkSurfaceFormatKHR> formats {};
        std::vector<VkPresentModeKHR> modes {};
    };

    SwapSupport query_swap_support(VkPhysicalDevice pd, VkSurfaceKHR surface) {
        SwapSupport s {};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surface, &s.caps);
        std::uint32_t count {0U};
        vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &count, nullptr);
        s.formats.resize(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &count, s.formats.data());
        count = 0U;
        vkGetPhysicalDevicePresentModesKHR(pd, surface, &count, nullptr);
        s.modes.resize(count);
        vkGetPhysicalDevicePresentModesKHR(pd, surface, &count, s.modes.data());
        return s;
    }

    VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& available) {
        for (const auto& f : available) {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return f;
            }
        }
        return available.front();
    }

    VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& available) {
        for (const auto m : available) {
            if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
                return m;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& caps, std::uint32_t width, std::uint32_t height) {
        if (caps.currentExtent.width != UINT32_MAX) {
            return caps.currentExtent;
        }
        VkExtent2D e {width, height};
        e.width = std::clamp(e.width, caps.minImageExtent.width, caps.maxImageExtent.width);
        e.height = std::clamp(e.height, caps.minImageExtent.height, caps.maxImageExtent.height);
        return e;
    }

    VkCommandBuffer begin_one_time_cmd(VkDevice device, VkCommandPool pool) {
        VkCommandBufferAllocateInfo alloc {};
        alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandPool = pool;
        alloc.commandBufferCount = 1;
        VkCommandBuffer cmd {};
        vkAllocateCommandBuffers(device, &alloc, &cmd);
        VkCommandBufferBeginInfo begin {};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &begin);
        return cmd;
    }

    void end_one_time_cmd(VkDevice device, VkQueue queue, VkCommandPool pool, VkCommandBuffer cmd) {
        vkEndCommandBuffer(cmd);
        VkSubmitInfo submit {};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);
        vkFreeCommandBuffers(device, pool, 1, &cmd);
    }

} // namespace

VulkanContext::VulkanContext(GLFWwindow* window, std::uint32_t width, std::uint32_t height) {
    create_instance();
    setup_debug();
    create_surface(window);
    pick_physical_device();
    create_logical_device();
    create_swapchain(width, height);
    create_image_views();
    create_render_pass();
    create_graphics_pipeline();
    create_framebuffers();
    create_command_pool();
    create_vertex_buffer();
    create_command_buffers();
    create_sync_objects();
}

VulkanContext::VulkanContext(VulkanContext&& other) noexcept {
    *this = std::move(other);
}

VulkanContext& VulkanContext::operator=(VulkanContext&& other) noexcept {
    if (this != &other) {
        this->~VulkanContext();
        m_instance = other.m_instance; other.m_instance = nullptr;
        m_surface = other.m_surface; other.m_surface = nullptr;
        m_physical_device = other.m_physical_device; other.m_physical_device = nullptr;
        m_device = other.m_device; other.m_device = nullptr;
        m_graphics_queue = other.m_graphics_queue; other.m_graphics_queue = nullptr;
        m_present_queue = other.m_present_queue; other.m_present_queue = nullptr;
        m_swapchain = other.m_swapchain; other.m_swapchain = nullptr;
        m_swapchain_image_views = std::move(other.m_swapchain_image_views);
        m_render_pass = other.m_render_pass; other.m_render_pass = nullptr;
        m_pipeline_layout = other.m_pipeline_layout; other.m_pipeline_layout = nullptr;
        m_pipeline = other.m_pipeline; other.m_pipeline = nullptr;
        m_framebuffers = std::move(other.m_framebuffers);
        m_command_pool = other.m_command_pool; other.m_command_pool = nullptr;
        m_command_buffers = std::move(other.m_command_buffers);
        m_vertex_buffer = other.m_vertex_buffer; other.m_vertex_buffer = nullptr;
        m_vertex_memory = other.m_vertex_memory; other.m_vertex_memory = nullptr;
        m_image_available = std::move(other.m_image_available);
        m_render_finished = std::move(other.m_render_finished);
        m_in_flight_fences = std::move(other.m_in_flight_fences);
        m_current_frame = other.m_current_frame; other.m_current_frame = 0U;
        m_extent = other.m_extent;
        m_device_name = std::move(other.m_device_name);
    }
    return *this;
}

VulkanContext::~VulkanContext() {
    if (m_device != nullptr) {
        vkDeviceWaitIdle(m_device);
        for (std::size_t i = 0; i < m_in_flight_fences.size(); ++i) {
            vkDestroyFence(m_device, m_in_flight_fences[i], nullptr);
            vkDestroySemaphore(m_device, m_render_finished[i], nullptr);
            vkDestroySemaphore(m_device, m_image_available[i], nullptr);
        }
        vkDestroyBuffer(m_device, m_vertex_buffer, nullptr);
        vkFreeMemory(m_device, m_vertex_memory, nullptr);
        vkDestroyCommandPool(m_device, m_command_pool, nullptr);
        cleanup_swapchain();
        vkDestroyDevice(m_device, nullptr);
    }
    if (m_surface != nullptr) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    }
    if (m_instance != nullptr) {
        if (m_debug_messenger != nullptr) {
            auto destroyMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroyMessenger != nullptr) {
                destroyMessenger(m_instance, reinterpret_cast<VkDebugUtilsMessengerEXT>(m_debug_messenger), nullptr);
            }
            m_debug_messenger = nullptr;
        }
        vkDestroyInstance(m_instance, nullptr);
    }
}

void VulkanContext::create_instance() {
    VkApplicationInfo app {};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "VulkanoCodex";
    app.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app.pEngineName = "NoEngine";
    app.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app.apiVersion = VK_API_VERSION_1_2;

    std::uint32_t glfwCount {0U};
    const char** glfwExt = glfwGetRequiredInstanceExtensions(&glfwCount);
    std::vector<const char*> extensions(glfwExt, glfwExt + glfwCount);
    if (kEnableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &app;
    info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();

    const std::array<const char*, 1> layers {"VK_LAYER_KHRONOS_validation"};
    if (kEnableValidation) {
        info.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
        info.ppEnabledLayerNames = layers.data();
    }
    if (vkCreateInstance(&info, nullptr, &m_instance) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create Vulkan instance"};
    }
}

void VulkanContext::setup_debug() {
    if (!kEnableValidation) {
        return;
    }
    auto createMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
    if (createMessenger != nullptr) {
        VkDebugUtilsMessengerCreateInfoEXT info {};
        info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        info.pfnUserCallback = debug_callback;
        VkDebugUtilsMessengerEXT messenger {};
        if (createMessenger(m_instance, &info, nullptr, &messenger) == VK_SUCCESS) {
            m_debug_messenger = messenger;
        }
    }
}

void VulkanContext::create_surface(GLFWwindow* window) {
    if (glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create window surface"};
    }
}

void VulkanContext::pick_physical_device() {
    std::uint32_t count {0U};
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (count == 0U) {
        throw std::runtime_error{"No Vulkan physical devices found"};
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());
    for (auto pd : devices) {
        QueueFamilies q = find_queue_families(pd, m_surface);
        if (!q.complete()) { continue; }
        std::uint32_t extCount {0U};
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, exts.data());
        bool swapchainSupported = std::all_of(kDeviceExtensions.begin(), kDeviceExtensions.end(), [&](const char* required) {
            return std::any_of(exts.begin(), exts.end(), [&](const VkExtensionProperties& e) { return std::strcmp(e.extensionName, required) == 0; });
        });
        if (!swapchainSupported) { continue; }
        m_physical_device = pd;
        VkPhysicalDeviceProperties props {};
        vkGetPhysicalDeviceProperties(pd, &props);
        m_device_name = props.deviceName;
        break;
    }
    if (m_physical_device == nullptr) {
        throw std::runtime_error{"Failed to find suitable physical device"};
    }
}

void VulkanContext::create_logical_device() {
    QueueFamilies q = find_queue_families(m_physical_device, m_surface);
    std::vector<VkDeviceQueueCreateInfo> queues;
    const std::array<std::uint32_t, 2> unique {q.graphics, q.present};
    std::vector<std::uint32_t> dedup;
    for (auto idx : unique) { if (std::find(dedup.begin(), dedup.end(), idx) == dedup.end()) { dedup.push_back(idx); } }
    float priority {1.0F};
    for (auto idx : dedup) {
        VkDeviceQueueCreateInfo qi {};
        qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = idx;
        qi.queueCount = 1;
        qi.pQueuePriorities = &priority;
        queues.push_back(qi);
    }
    VkPhysicalDeviceFeatures features {};
    VkDeviceCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = static_cast<std::uint32_t>(queues.size());
    info.pQueueCreateInfos = queues.data();
    info.pEnabledFeatures = &features;
    info.enabledExtensionCount = static_cast<std::uint32_t>(kDeviceExtensions.size());
    info.ppEnabledExtensionNames = kDeviceExtensions.data();
    const std::array<const char*, 1> layers {"VK_LAYER_KHRONOS_validation"};
    if (kEnableValidation) {
        info.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
        info.ppEnabledLayerNames = layers.data();
    }
    if (vkCreateDevice(m_physical_device, &info, nullptr, &m_device) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create logical device"};
    }
    vkGetDeviceQueue(m_device, q.graphics, 0, &m_graphics_queue);
    vkGetDeviceQueue(m_device, q.present, 0, &m_present_queue);
}

void VulkanContext::create_swapchain(std::uint32_t width, std::uint32_t height) {
    SwapSupport s = query_swap_support(m_physical_device, m_surface);
    const VkSurfaceFormatKHR surface_format = choose_surface_format(s.formats);
    const VkPresentModeKHR present_mode = choose_present_mode(s.modes);
    const VkExtent2D extent = choose_extent(s.caps, width, height);
    std::uint32_t image_count = s.caps.minImageCount + 1;
    if (s.caps.maxImageCount > 0 && image_count > s.caps.maxImageCount) {
        image_count = s.caps.maxImageCount;
    }
    VkSwapchainCreateInfoKHR info {};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = m_surface;
    info.minImageCount = image_count;
    info.imageFormat = surface_format.format;
    info.imageColorSpace = surface_format.colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    QueueFamilies q = find_queue_families(m_physical_device, m_surface);
    std::uint32_t indices[] = {q.graphics, q.present};
    if (q.graphics != q.present) {
        info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices = indices;
    } else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    info.preTransform = s.caps.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = present_mode;
    info.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(m_device, &info, nullptr, &m_swapchain) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create swapchain"};
    }
    m_extent = SwapchainExtent{extent.width, extent.height};
}

void VulkanContext::create_image_views() {
    std::uint32_t count {0U};
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &count, nullptr);
    std::vector<VkImage> images(count);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &count, images.data());
    m_swapchain_image_views.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        VkImageViewCreateInfo info {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = images[i];
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = VK_FORMAT_B8G8R8A8_SRGB;
        info.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;
        if (vkCreateImageView(m_device, &info, nullptr, &m_swapchain_image_views[i]) != VK_SUCCESS) {
            throw std::runtime_error{"Failed to create image view"};
        }
    }
}

void VulkanContext::create_render_pass() {
    VkAttachmentDescription color {};
    color.format = VK_FORMAT_B8G8R8A8_SRGB;
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
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 1> attachments {color};
    VkRenderPassCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    info.pAttachments = attachments.data();
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dep;
    if (vkCreateRenderPass(m_device, &info, nullptr, &m_render_pass) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create render pass"};
    }
}

void VulkanContext::create_graphics_pipeline() {
    const auto vert = shaders::vertex_spirv();
    const auto frag = shaders::fragment_spirv();
    VkShaderModule vert_mod = create_shader_module(m_device, vert);
    VkShaderModule frag_mod = create_shader_module(m_device, frag);

    VkPipelineShaderStageCreateInfo vs {};
    vs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vs.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vs.module = vert_mod;
    vs.pName = "main";

    VkPipelineShaderStageCreateInfo fs {};
    fs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fs.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fs.module = frag_mod;
    fs.pName = "main";
    VkPipelineShaderStageCreateInfo stages[] = {vs, fs};

    VkVertexInputBindingDescription binding {};
    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attr {};
    attr.location = 0;
    attr.binding = 0;
    attr.format = VK_FORMAT_R32G32_SFLOAT;
    attr.offset = 0;
    VkPipelineVertexInputStateCreateInfo vertex_input {};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = 1;
    vertex_input.pVertexAttributeDescriptions = &attr;

    VkPipelineInputAssemblyStateCreateInfo input_assembly {};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport {};
    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = static_cast<float>(m_extent.width);
    viewport.height = static_cast<float>(m_extent.height);
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;
    VkRect2D scissor {{0, 0}, {m_extent.width, m_extent.height}};
    VkPipelineViewportStateCreateInfo viewport_state {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo raster {};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.depthClampEnable = VK_FALSE;
    raster.rasterizerDiscardEnable = VK_FALSE;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.lineWidth = 1.0F;
    raster.cullMode = VK_CULL_MODE_BACK_BIT;
    raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
    raster.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo msaa {};
    msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_att {};
    color_blend_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_att.blendEnable = VK_FALSE;
    VkPipelineColorBlendStateCreateInfo color_blend {};
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.attachmentCount = 1;
    color_blend.pAttachments = &color_blend_att;

    VkPushConstantRange push_range {};
    push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    push_range.offset = 0;
    push_range.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo layout_info {};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_range;
    if (vkCreatePipelineLayout(m_device, &layout_info, nullptr, &m_pipeline_layout) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create pipeline layout"};
    }

    VkGraphicsPipelineCreateInfo pipe {};
    pipe.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipe.stageCount = 2;
    pipe.pStages = stages;
    pipe.pVertexInputState = &vertex_input;
    pipe.pInputAssemblyState = &input_assembly;
    pipe.pViewportState = &viewport_state;
    pipe.pRasterizationState = &raster;
    pipe.pMultisampleState = &msaa;
    pipe.pDepthStencilState = nullptr;
    pipe.pColorBlendState = &color_blend;
    pipe.layout = m_pipeline_layout;
    pipe.renderPass = m_render_pass;
    pipe.subpass = 0;
    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipe, nullptr, &m_pipeline) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create graphics pipeline"};
    }
    vkDestroyShaderModule(m_device, frag_mod, nullptr);
    vkDestroyShaderModule(m_device, vert_mod, nullptr);
}

void VulkanContext::create_framebuffers() {
    m_framebuffers.resize(m_swapchain_image_views.size());
    for (std::size_t i = 0; i < m_swapchain_image_views.size(); ++i) {
        VkImageView attachments[] = {m_swapchain_image_views[i]};
        VkFramebufferCreateInfo info {};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = m_render_pass;
        info.attachmentCount = 1;
        info.pAttachments = attachments;
        info.width = m_extent.width;
        info.height = m_extent.height;
        info.layers = 1;
        if (vkCreateFramebuffer(m_device, &info, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error{"Failed to create framebuffer"};
        }
    }
}

void VulkanContext::create_command_pool() {
    QueueFamilies q = find_queue_families(m_physical_device, m_surface);
    VkCommandPoolCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.queueFamilyIndex = q.graphics;
    info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(m_device, &info, nullptr, &m_command_pool) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create command pool"};
    }
}

void VulkanContext::create_vertex_buffer() {
    const std::array<Vertex, 3> vertices {Vertex{glm::vec2{-0.5F, -0.5F}}, Vertex{glm::vec2{0.5F, -0.5F}}, Vertex{glm::vec2{0.0F, 0.5F}}};
    VkBufferCreateInfo buf {};
    buf.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf.size = sizeof(vertices);
    buf.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buf.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(m_device, &buf, nullptr, &m_vertex_buffer) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create vertex buffer"};
    }
    VkMemoryRequirements memReq {};
    vkGetBufferMemoryRequirements(m_device, m_vertex_buffer, &memReq);
    VkPhysicalDeviceMemoryProperties memProps {};
    vkGetPhysicalDeviceMemoryProperties(m_physical_device, &memProps);
    std::uint32_t typeIndex = UINT32_MAX;
    for (std::uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memReq.memoryTypeBits & (1 << i)) != 0U && (memProps.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            typeIndex = i;
            break;
        }
    }
    if (typeIndex == UINT32_MAX) {
        throw std::runtime_error{"Suitable memory type not found"};
    }
    VkMemoryAllocateInfo alloc {};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = memReq.size;
    alloc.memoryTypeIndex = typeIndex;
    if (vkAllocateMemory(m_device, &alloc, nullptr, &m_vertex_memory) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to allocate vertex memory"};
    }
    vkBindBufferMemory(m_device, m_vertex_buffer, m_vertex_memory, 0);
    void* data {nullptr};
    vkMapMemory(m_device, m_vertex_memory, 0, sizeof(vertices), 0, &data);
    std::memcpy(data, vertices.data(), sizeof(vertices));
    vkUnmapMemory(m_device, m_vertex_memory);
}

void VulkanContext::create_command_buffers() {
    m_command_buffers.resize(m_framebuffers.size());
    VkCommandBufferAllocateInfo alloc {};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = m_command_pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = static_cast<std::uint32_t>(m_command_buffers.size());
    vkAllocateCommandBuffers(m_device, &alloc, reinterpret_cast<VkCommandBuffer*>(m_command_buffers.data()));
}

void VulkanContext::create_sync_objects() {
    m_image_available.resize(kMaxFramesInFlight);
    m_render_finished.resize(kMaxFramesInFlight);
    m_in_flight_fences.resize(kMaxFramesInFlight);
    VkSemaphoreCreateInfo sem {};
    sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fen {};
    fen.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fen.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (std::size_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (vkCreateSemaphore(m_device, &sem, nullptr, &m_image_available[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_device, &sem, nullptr, &m_render_finished[i]) != VK_SUCCESS ||
            vkCreateFence(m_device, &fen, nullptr, &m_in_flight_fences[i]) != VK_SUCCESS) {
            throw std::runtime_error{"Failed to create sync objects"};
        }
    }
}

void VulkanContext::cleanup_swapchain() {
    for (auto fb : m_framebuffers) { vkDestroyFramebuffer(m_device, fb, nullptr); }
    m_framebuffers.clear();
    if (m_pipeline != nullptr) { vkDestroyPipeline(m_device, m_pipeline, nullptr); m_pipeline = nullptr; }
    if (m_pipeline_layout != nullptr) { vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr); m_pipeline_layout = nullptr; }
    if (m_render_pass != nullptr) { vkDestroyRenderPass(m_device, m_render_pass, nullptr); m_render_pass = nullptr; }
    for (auto iv : m_swapchain_image_views) { vkDestroyImageView(m_device, iv, nullptr); }
    m_swapchain_image_views.clear();
    if (m_swapchain != nullptr) { vkDestroySwapchainKHR(m_device, m_swapchain, nullptr); m_swapchain = nullptr; }
}

void VulkanContext::wait_idle() const noexcept { vkDeviceWaitIdle(m_device); }

void VulkanContext::recreate_swapchain(std::uint32_t width, std::uint32_t height) {
    vkDeviceWaitIdle(m_device);
    cleanup_swapchain();
    create_swapchain(width, height);
    create_image_views();
    create_render_pass();
    create_graphics_pipeline();
    create_framebuffers();
    create_command_buffers();
}

void VulkanContext::draw_frame(const glm::vec4& color, bool& swapchain_recreated, void (*imgui_render)(void* cmd_buf, void* user), void* user_ptr) {
    vkWaitForFences(m_device, 1, &m_in_flight_fences[m_current_frame], VK_TRUE, UINT64_MAX);
    vkResetFences(m_device, 1, &m_in_flight_fences[m_current_frame]);

    std::uint32_t image_index {0U};
    VkResult acq = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_image_available[m_current_frame], VK_NULL_HANDLE, &image_index);
    if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
        swapchain_recreated = true;
        return;
    }
    if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error{"Failed to acquire swapchain image"};
    }

    const VkCommandBuffer cmd = reinterpret_cast<VkCommandBuffer>(m_command_buffers[image_index]);
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo begin {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &begin);

    const std::array<VkClearValue, 1> clear {{{{0.0F, 0.0F, 0.0F, 1.0F}}}};
    VkRenderPassBeginInfo rp {};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = m_render_pass;
    rp.framebuffer = m_framebuffers[image_index];
    rp.renderArea.offset = {0, 0};
    rp.renderArea.extent = {m_extent.width, m_extent.height};
    rp.clearValueCount = static_cast<std::uint32_t>(clear.size());
    rp.pClearValues = clear.data();
    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    const VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertex_buffer, offsets);
    PushConstants pc {color};
    vkCmdPushConstants(cmd, m_pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pc);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    if (imgui_render != nullptr) {
        imgui_render(reinterpret_cast<void*>(cmd), user_ptr);
    }

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkSemaphore wait_sems[] = {m_image_available[m_current_frame]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signal_sems[] = {m_render_finished[m_current_frame]};
    VkSubmitInfo submit {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = wait_sems;
    submit.pWaitDstStageMask = wait_stages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = signal_sems;
    if (vkQueueSubmit(m_graphics_queue, 1, &submit, m_in_flight_fences[m_current_frame]) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to submit draw"};
    }

    VkSwapchainKHR swapchains[] = {m_swapchain};
    VkPresentInfoKHR present {};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = signal_sems;
    present.swapchainCount = 1;
    present.pSwapchains = swapchains;
    present.pImageIndices = &image_index;
    const VkResult pres = vkQueuePresentKHR(m_present_queue, &present);
    if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) {
        swapchain_recreated = true;
    } else if (pres != VK_SUCCESS) {
        throw std::runtime_error{"Failed to present"};
    }
    m_current_frame = (m_current_frame + 1) % kMaxFramesInFlight;
}

const char* VulkanContext::device_name() const noexcept { return m_device_name.c_str(); }
SwapchainExtent VulkanContext::current_extent() const noexcept { return m_extent; }
VkDevice VulkanContext::device() const noexcept { return m_device; }
VkRenderPass VulkanContext::render_pass() const noexcept { return m_render_pass; }
VkInstance VulkanContext::instance() const noexcept { return m_instance; }
void* VulkanContext::graphics_queue() const noexcept { return m_graphics_queue; }

} // namespace vulkano
