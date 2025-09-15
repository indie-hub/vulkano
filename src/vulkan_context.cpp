#include <vulkano/vulkan_context.hpp>
#include <vulkano/window.hpp>
#include <vulkano/icosphere.hpp>
#include <vulkano/mesh.hpp>

#if VULKANO_HAS_VULKAN
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <array>
#include <functional>
#include <vector>
#include <stdexcept>
#include <cstdio>
#include <cstring>

namespace vulkano {

struct VulkanContext::Impl final {
    VkInstance instance {VK_NULL_HANDLE};
    VkSurfaceKHR surface {VK_NULL_HANDLE};
    VkPhysicalDevice physical_device {VK_NULL_HANDLE};
    VkDevice device {VK_NULL_HANDLE};
    VkQueue graphics_queue {VK_NULL_HANDLE};
    uint32_t graphics_queue_family {0U};

    VkSwapchainKHR swapchain {VK_NULL_HANDLE};
    VkFormat swapchain_format {VK_FORMAT_B8G8R8A8_UNORM};
    VkColorSpaceKHR swapchain_color_space {VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    VkExtent2D swapchain_extent { };
    std::vector<VkImage> swapchain_images { };
    std::vector<VkImageView> swapchain_image_views { };

    VkRenderPass render_pass {VK_NULL_HANDLE};
    std::vector<VkFramebuffer> framebuffers { };
    VkCommandPool command_pool {VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> command_buffers { };
    VkSemaphore image_available {VK_NULL_HANDLE};
    VkSemaphore render_finished {VK_NULL_HANDLE};
    VkFence in_flight {VK_NULL_HANDLE};

    // Optional UI callback invoked during command recording
    std::function<void(void* cmd_buffer)> ui_renderer { };

    // Mesh pipeline and GPU buffers (created only if shaders are available)
    VkPipelineLayout pipeline_layout {VK_NULL_HANDLE};
    VkPipeline pipeline {VK_NULL_HANDLE};
    VkBuffer vertex_buffer {VK_NULL_HANDLE};
    VkDeviceMemory vertex_memory {VK_NULL_HANDLE};
    VkBuffer index_buffer {VK_NULL_HANDLE};
    VkDeviceMemory index_memory {VK_NULL_HANDLE};
    std::uint32_t index_count {0U};
    std::uint32_t current_subdivisions {0U};

    Impl() = default;
    ~Impl() = default;
};

struct VulkanContextInternalsHelper final { static VulkanContext::Impl& get(VulkanContext& ctx) noexcept { return *ctx.impl; } };

void* VulkanContextAccess::impl_ptr(VulkanContext& ctx) noexcept { return static_cast<void*>(&VulkanContextInternalsHelper::get(ctx)); }
void* VulkanContextAccess::instance(VulkanContext& ctx) noexcept { return static_cast<void*>(ctx.impl->instance); }
void* VulkanContextAccess::physical(VulkanContext& ctx) noexcept { return static_cast<void*>(ctx.impl->physical_device); }
void* VulkanContextAccess::device(VulkanContext& ctx) noexcept { return static_cast<void*>(ctx.impl->device); }
void* VulkanContextAccess::queue(VulkanContext& ctx) noexcept { return static_cast<void*>(ctx.impl->graphics_queue); }
std::uint32_t VulkanContextAccess::queue_family(VulkanContext& ctx) noexcept { return ctx.impl->graphics_queue_family; }
void* VulkanContextAccess::render_pass(VulkanContext& ctx) noexcept { return static_cast<void*>(ctx.impl->render_pass); }
std::uint32_t VulkanContextAccess::image_count(VulkanContext& ctx) noexcept { return static_cast<uint32_t>(ctx.impl->swapchain_images.size()); }

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
    // Simple stderr print would go here in future
    (void)pCallbackData;
    return VK_FALSE;
}

static void create_instance(VkInstance* instance) {
    VkApplicationInfo app_info {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "vulkano";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "vulkano";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    uint32_t ext_count = 0U;
    const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);
    std::vector<const char*> extensions {};
    extensions.reserve(static_cast<std::size_t>(ext_count + 1U));
    for (uint32_t i = 0; i < ext_count; ++i) {
        extensions.push_back(exts[i]);
    }
#ifndef NDEBUG
    // Enable debug utils when validation is enabled
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    VkInstanceCreateInfo ci {};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app_info;
    ci.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();

#ifndef NDEBUG
    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
    ci.enabledLayerCount = 1U;
    ci.ppEnabledLayerNames = layers;
#endif

    const VkResult res = vkCreateInstance(&ci, nullptr, instance);
    if (res != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create Vulkan instance"};
    }
}

static bool query_present_support(VkPhysicalDevice pd, uint32_t family, VkSurfaceKHR surface) {
    VkBool32 supported = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(pd, family, surface, &supported);
    return supported == VK_TRUE;
}

static VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    return formats.front();
}

static VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& modes) {
    for (const auto m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
            return m;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& caps, GLFWwindow* window) {
    if (caps.currentExtent.width != UINT32_MAX) {
        return caps.currentExtent;
    }
    int w = 0;
    int h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    VkExtent2D extent {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
    extent.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, extent.width));
    extent.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, extent.height));
    return extent;
}

static void create_swapchain_and_views(VulkanContext& ctx, GLFWwindow* window) {
    auto& impl = VulkanContextInternalsHelper::get(ctx);
    VkSurfaceCapabilitiesKHR caps {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(impl.physical_device, impl.surface, &caps);
    uint32_t fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(impl.physical_device, impl.surface, &fmt_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmt_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(impl.physical_device, impl.surface, &fmt_count, formats.data());

    uint32_t pm_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(impl.physical_device, impl.surface, &pm_count, nullptr);
    std::vector<VkPresentModeKHR> modes(pm_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(impl.physical_device, impl.surface, &pm_count, modes.data());

    const VkSurfaceFormatKHR chosen = choose_surface_format(formats);
    const VkPresentModeKHR present_mode = choose_present_mode(modes);
    const VkExtent2D extent = choose_extent(caps, window);

    uint32_t image_count = caps.minImageCount + 1U;
    if (caps.maxImageCount > 0U && image_count > caps.maxImageCount) {
        image_count = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR sci {};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = impl.surface;
    sci.minImageCount = image_count;
    sci.imageFormat = chosen.format;
    sci.imageColorSpace = chosen.colorSpace;
    sci.imageExtent = extent;
    sci.imageArrayLayers = 1U;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = present_mode;
    sci.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(impl.device, &sci, nullptr, &impl.swapchain) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create swapchain"};
    }
    impl.swapchain_format = chosen.format;
    impl.swapchain_color_space = chosen.colorSpace;
    impl.swapchain_extent = extent;

    uint32_t sc_image_count = 0U;
    vkGetSwapchainImagesKHR(impl.device, impl.swapchain, &sc_image_count, nullptr);
    impl.swapchain_images.resize(sc_image_count);
    vkGetSwapchainImagesKHR(impl.device, impl.swapchain, &sc_image_count, impl.swapchain_images.data());

    impl.swapchain_image_views.resize(sc_image_count);
    for (uint32_t i = 0; i < sc_image_count; ++i) {
        VkImageViewCreateInfo ivci {};
        ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image = impl.swapchain_images[i];
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = impl.swapchain_format;
        ivci.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel = 0;
        ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount = 1;
        if (vkCreateImageView(impl.device, &ivci, nullptr, &impl.swapchain_image_views[i]) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create image view"};
        }
    }
}

static void create_renderpass_and_framebuffers(VulkanContext& ctx) {
    auto& impl = VulkanContextInternalsHelper::get(ctx);
    VkAttachmentDescription color {};
    color.format = impl.swapchain_format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_ref {};
    color_ref.attachment = 0U;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1U;
    subpass.pColorAttachments = &color_ref;

    VkSubpassDependency dep {};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0U;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci {};
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 1U;
    rpci.pAttachments = &color;
    rpci.subpassCount = 1U;
    rpci.pSubpasses = &subpass;
    rpci.dependencyCount = 1U;
    rpci.pDependencies = &dep;
    if (vkCreateRenderPass(impl.device, &rpci, nullptr, &impl.render_pass) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create render pass"};
    }

    impl.framebuffers.resize(impl.swapchain_image_views.size());
    for (std::size_t i = 0; i < impl.swapchain_image_views.size(); ++i) {
        VkImageView attachments[] = {impl.swapchain_image_views[i]};
        VkFramebufferCreateInfo fbci {};
        fbci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass = impl.render_pass;
        fbci.attachmentCount = 1U;
        fbci.pAttachments = attachments;
        fbci.width = impl.swapchain_extent.width;
        fbci.height = impl.swapchain_extent.height;
        fbci.layers = 1U;
        if (vkCreateFramebuffer(impl.device, &fbci, nullptr, &impl.framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create framebuffer"};
        }
    }
}

static void create_commands_and_sync(VulkanContext& ctx) {
    auto& impl = VulkanContextInternalsHelper::get(ctx);
    VkCommandPoolCreateInfo cpci {};
    cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.queueFamilyIndex = impl.graphics_queue_family;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(impl.device, &cpci, nullptr, &impl.command_pool) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create command pool"};
    }
    impl.command_buffers.resize(impl.framebuffers.size());
    VkCommandBufferAllocateInfo cbai {};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = impl.command_pool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = static_cast<uint32_t>(impl.command_buffers.size());
    if (vkAllocateCommandBuffers(impl.device, &cbai, impl.command_buffers.data()) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to allocate command buffers"};
    }

    VkSemaphoreCreateInfo sci {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    if (vkCreateSemaphore(impl.device, &sci, nullptr, &impl.image_available) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create semaphore"};
    }
    if (vkCreateSemaphore(impl.device, &sci, nullptr, &impl.render_finished) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create semaphore"};
    }
    VkFenceCreateInfo fci {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateFence(impl.device, &fci, nullptr, &impl.in_flight) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create fence"};
    }
}

static void record_clear_cmds(VulkanContext& ctx) {
    auto& impl = VulkanContextInternalsHelper::get(ctx);
    for (std::size_t i = 0; i < impl.command_buffers.size(); ++i) {
        VkCommandBufferBeginInfo bi {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(impl.command_buffers[i], &bi);
        VkClearValue clear_color {};
        clear_color.color = {{0.05F, 0.15F, 0.30F, 1.0F}};
        VkRenderPassBeginInfo rpbi {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpbi.renderPass = impl.render_pass;
        rpbi.framebuffer = impl.framebuffers[i];
        rpbi.renderArea.offset = {0, 0};
        rpbi.renderArea.extent = impl.swapchain_extent;
        rpbi.clearValueCount = 1U;
        rpbi.pClearValues = &clear_color;
        vkCmdBeginRenderPass(impl.command_buffers[i], &rpbi, VK_SUBPASS_CONTENTS_INLINE);
        if (impl.ui_renderer) {
            impl.ui_renderer(static_cast<void*>(impl.command_buffers[i]));
        }
        vkCmdEndRenderPass(impl.command_buffers[i]);
        vkEndCommandBuffer(impl.command_buffers[i]);
    }
}

// Minimal file loader for SPIR-V binaries
static std::vector<char> load_binary_file(const char* path) {
    std::vector<char> data {};
    FILE* f = std::fopen(path, "rb");
    if (f == nullptr) {
        return data;
    }
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size > 0) {
        data.resize(static_cast<std::size_t>(size));
        std::fread(data.data(), 1, static_cast<std::size_t>(size), f);
    }
    std::fclose(f);
    return data;
}

static VkShaderModule create_shader_module(VkDevice device, const std::vector<char>& bytes) {
    VkShaderModule mod {VK_NULL_HANDLE};
    if (bytes.empty()) {
        return mod;
    }
    VkShaderModuleCreateInfo ci {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = bytes.size();
    ci.pCode = reinterpret_cast<const uint32_t*>(bytes.data());
    if (vkCreateShaderModule(device, &ci, nullptr, &mod) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return mod;
}

static void destroy_mesh_buffers(VulkanContext& ctx) {
    auto& impl = VulkanContextInternalsHelper::get(ctx);
    if (impl.vertex_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(impl.device, impl.vertex_buffer, nullptr);
        impl.vertex_buffer = VK_NULL_HANDLE;
    }
    if (impl.vertex_memory != VK_NULL_HANDLE) {
        vkFreeMemory(impl.device, impl.vertex_memory, nullptr);
        impl.vertex_memory = VK_NULL_HANDLE;
    }
    if (impl.index_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(impl.device, impl.index_buffer, nullptr);
        impl.index_buffer = VK_NULL_HANDLE;
    }
    if (impl.index_memory != VK_NULL_HANDLE) {
        vkFreeMemory(impl.device, impl.index_memory, nullptr);
        impl.index_memory = VK_NULL_HANDLE;
    }
    impl.index_count = 0U;
}

static uint32_t find_memory_type(VkPhysicalDevice pd, uint32_t typeBits, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp {};
    vkGetPhysicalDeviceMemoryProperties(pd, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((typeBits & (1U << i)) != 0U && (mp.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    return UINT32_MAX;
}

static void create_buffer(VulkanContext& ctx, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer* outBuffer, VkDeviceMemory* outMemory) {
    auto& impl = VulkanContextInternalsHelper::get(ctx);
    VkBufferCreateInfo bci {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(impl.device, &bci, nullptr, outBuffer) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create buffer"};
    }
    VkMemoryRequirements req {};
    vkGetBufferMemoryRequirements(impl.device, *outBuffer, &req);
    const uint32_t mem_type = find_memory_type(impl.physical_device, req.memoryTypeBits, props);
    if (mem_type == UINT32_MAX) {
        throw std::runtime_error {"Failed to find suitable memory type"};
    }
    VkMemoryAllocateInfo mai {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = mem_type;
    if (vkAllocateMemory(impl.device, &mai, nullptr, outMemory) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to allocate buffer memory"};
    }
    vkBindBufferMemory(impl.device, *outBuffer, *outMemory, 0);
}

static void upload_mesh_to_gpu(VulkanContext& ctx, const vulkano::Mesh& mesh) {
    auto& impl = VulkanContextInternalsHelper::get(ctx);
    destroy_mesh_buffers(ctx);

    const VkDeviceSize vsize = static_cast<VkDeviceSize>(mesh.vertices.size() * sizeof(vulkano::Vertex));
    const VkDeviceSize isize = static_cast<VkDeviceSize>(mesh.indices.size() * sizeof(std::uint32_t));
    create_buffer(ctx, vsize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &impl.vertex_buffer, &impl.vertex_memory);
    create_buffer(ctx, isize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &impl.index_buffer, &impl.index_memory);

    void* data = nullptr;
    vkMapMemory(impl.device, impl.vertex_memory, 0, vsize, 0, &data);
    std::memcpy(data, mesh.vertices.data(), static_cast<size_t>(vsize));
    vkUnmapMemory(impl.device, impl.vertex_memory);

    data = nullptr;
    vkMapMemory(impl.device, impl.index_memory, 0, isize, 0, &data);
    std::memcpy(data, mesh.indices.data(), static_cast<size_t>(isize));
    vkUnmapMemory(impl.device, impl.index_memory);

    impl.index_count = static_cast<std::uint32_t>(mesh.indices.size());
}

static void destroy_pipeline(VulkanContext& ctx) {
    auto& impl = VulkanContextInternalsHelper::get(ctx);
    if (impl.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(impl.device, impl.pipeline, nullptr);
        impl.pipeline = VK_NULL_HANDLE;
    }
    if (impl.pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(impl.device, impl.pipeline_layout, nullptr);
        impl.pipeline_layout = VK_NULL_HANDLE;
    }
}

static void create_pipeline(VulkanContext& ctx) {
    auto& impl = VulkanContextInternalsHelper::get(ctx);
    // Expect SPIR-V shaders at runtime; if not present, skip pipeline creation
    const std::vector<char> vs = load_binary_file("shaders/simple.vert.spv");
    const std::vector<char> fs = load_binary_file("shaders/simple.frag.spv");
    if (vs.empty() || fs.empty()) {
        destroy_pipeline(ctx);
        return;
    }
    VkShaderModule vmod = create_shader_module(impl.device, vs);
    VkShaderModule fmod = create_shader_module(impl.device, fs);
    if (vmod == VK_NULL_HANDLE || fmod == VK_NULL_HANDLE) {
        if (vmod != VK_NULL_HANDLE) { vkDestroyShaderModule(impl.device, vmod, nullptr); }
        if (fmod != VK_NULL_HANDLE) { vkDestroyShaderModule(impl.device, fmod, nullptr); }
        destroy_pipeline(ctx);
        return;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vmod;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fmod;
    stages[1].pName = "main";

    // Vertex input: position, normal, tangent, bitangent, texcoord
    VkVertexInputBindingDescription binding {};
    binding.binding = 0;
    binding.stride = sizeof(vulkano::Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 5> attrs {};
    attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vulkano::Vertex, position)};
    attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vulkano::Vertex, normal)};
    attrs[2] = {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vulkano::Vertex, tangent)};
    attrs[3] = {3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vulkano::Vertex, bitangent)};
    attrs[4] = {4, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(vulkano::Vertex, texcoord)};

    VkPipelineVertexInputStateCreateInfo vi {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &binding;
    vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vi.pVertexAttributeDescriptions = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo ia {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport {};
    viewport.x = 0.0F; viewport.y = 0.0F;
    viewport.width = static_cast<float>(impl.swapchain_extent.width);
    viewport.height = static_cast<float>(impl.swapchain_extent.height);
    viewport.minDepth = 0.0F; viewport.maxDepth = 1.0F;
    VkRect2D scissor {{0,0}, impl.swapchain_extent};
    VkPipelineViewportStateCreateInfo vp {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1; vp.pViewports = &viewport;
    vp.scissorCount = 1; vp.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rs {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0F;

    VkPipelineMultisampleStateCreateInfo ms {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cba {};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1; cb.pAttachments = &cba;

    VkPushConstantRange pcr {};
    pcr.offset = 0U;
    pcr.size = static_cast<uint32_t>(sizeof(float) * 16U);
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo plci {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.pushConstantRangeCount = 1U;
    plci.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(impl.device, &plci, nullptr, &impl.pipeline_layout) != VK_SUCCESS) {
        vkDestroyShaderModule(impl.device, vmod, nullptr);
        vkDestroyShaderModule(impl.device, fmod, nullptr);
        destroy_pipeline(ctx);
        return;
    }

    VkGraphicsPipelineCreateInfo gpci {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gpci.stageCount = 2; gpci.pStages = stages;
    gpci.pVertexInputState = &vi;
    gpci.pInputAssemblyState = &ia;
    gpci.pViewportState = &vp;
    gpci.pRasterizationState = &rs;
    gpci.pMultisampleState = &ms;
    gpci.pColorBlendState = &cb;
    gpci.layout = impl.pipeline_layout;
    gpci.renderPass = impl.render_pass;
    gpci.subpass = 0;

    if (vkCreateGraphicsPipelines(impl.device, VK_NULL_HANDLE, 1, &gpci, nullptr, &impl.pipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(impl.device, vmod, nullptr);
        vkDestroyShaderModule(impl.device, fmod, nullptr);
        destroy_pipeline(ctx);
        return;
    }

    vkDestroyShaderModule(impl.device, vmod, nullptr);
    vkDestroyShaderModule(impl.device, fmod, nullptr);
}

VulkanContext::VulkanContext(const Window& window)
    : impl {std::make_unique<Impl>()} {
    create_instance(&impl->instance);
    GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window.native_handle());
    if (glfw_win == nullptr) {
        throw std::runtime_error {"Invalid GLFW window handle"};
    }
    if (glfwCreateWindowSurface(impl->instance, glfw_win, nullptr, &impl->surface) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create Vulkan surface"};
    }
    // Physical device selection (very naive)
    uint32_t count = 0U;
    vkEnumeratePhysicalDevices(impl->instance, &count, nullptr);
    if (count == 0U) {
        throw std::runtime_error {"No Vulkan physical devices found"};
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(impl->instance, &count, devices.data());
    impl->physical_device = devices[0];

    // Queue family selection with present support
    uint32_t qcount = 0U;
    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, nullptr);
    std::vector<VkQueueFamilyProperties> qprops(qcount);
    vkGetPhysicalDeviceQueueFamilyProperties(impl->physical_device, &qcount, qprops.data());
    uint32_t gfx_index = UINT32_MAX;
    for (uint32_t i = 0; i < qcount; ++i) {
        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U && query_present_support(impl->physical_device, i, impl->surface)) {
            gfx_index = i;
            break;
        }
    }
    if (gfx_index == UINT32_MAX) {
        throw std::runtime_error {"No graphics queue family found"};
    }
    impl->graphics_queue_family = gfx_index;

    const float priority = 1.0F;
    VkDeviceQueueCreateInfo qci {};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = gfx_index;
    qci.queueCount = 1U;
    qci.pQueuePriorities = &priority;

    VkDeviceCreateInfo dci {};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1U;
    dci.pQueueCreateInfos = &qci;
    const char* dev_exts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    dci.enabledExtensionCount = 1U;
    dci.ppEnabledExtensionNames = dev_exts;

    if (vkCreateDevice(impl->physical_device, &dci, nullptr, &impl->device) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create logical device"};
    }
    vkGetDeviceQueue(impl->device, gfx_index, 0U, &impl->graphics_queue);

    create_swapchain_and_views(*this, glfw_win);
    create_renderpass_and_framebuffers(*this);
    create_commands_and_sync(*this);
    record_clear_cmds(*this);
}

VulkanContext::~VulkanContext() {
    if (impl != nullptr) {
        if (impl->device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(impl->device);
        }
        destroy_pipeline(*this);
        destroy_mesh_buffers(*this);
        if (impl->in_flight != VK_NULL_HANDLE) {
            vkDestroyFence(impl->device, impl->in_flight, nullptr);
        }
        if (impl->image_available != VK_NULL_HANDLE) {
            vkDestroySemaphore(impl->device, impl->image_available, nullptr);
        }
        if (impl->render_finished != VK_NULL_HANDLE) {
            vkDestroySemaphore(impl->device, impl->render_finished, nullptr);
        }
        if (impl->command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(impl->device, impl->command_pool, nullptr);
        }
        for (auto fb : impl->framebuffers) {
            vkDestroyFramebuffer(impl->device, fb, nullptr);
        }
        if (impl->render_pass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(impl->device, impl->render_pass, nullptr);
        }
        for (auto v : impl->swapchain_image_views) {
            vkDestroyImageView(impl->device, v, nullptr);
        }
        if (impl->swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(impl->device, impl->swapchain, nullptr);
        }
        if (impl->device != VK_NULL_HANDLE) {
            vkDestroyDevice(impl->device, nullptr);
        }
        if (impl->surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(impl->instance, impl->surface, nullptr);
        }
        if (impl->instance != VK_NULL_HANDLE) {
            vkDestroyInstance(impl->instance, nullptr);
        }
    }
}

VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;

void VulkanContext::wait_idle() const noexcept {
    if (impl->device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(impl->device);
    }
}

void VulkanContext::draw_frame() {
    vkWaitForFences(impl->device, 1U, &impl->in_flight, VK_TRUE, UINT64_MAX);
    vkResetFences(impl->device, 1U, &impl->in_flight);

    uint32_t image_index = 0U;
    vkAcquireNextImageKHR(impl->device, impl->swapchain, UINT64_MAX, impl->image_available, VK_NULL_HANDLE, &image_index);

    // Re-record command buffer for this image to allow dynamic UI rendering
    {
        VkCommandBuffer cmd = impl->command_buffers[image_index];
        VkCommandBufferBeginInfo bi {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(cmd, &bi);
        VkClearValue clear_color {};
        clear_color.color = {{0.05F, 0.15F, 0.30F, 1.0F}};
        VkRenderPassBeginInfo rpbi {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpbi.renderPass = impl->render_pass;
        rpbi.framebuffer = impl->framebuffers[image_index];
        rpbi.renderArea.offset = {0, 0};
        rpbi.renderArea.extent = impl->swapchain_extent;
        rpbi.clearValueCount = 1U;
        rpbi.pClearValues = &clear_color;
        vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
        // Push a simple MVP matrix (perspective * view * model) for basic 3D
        {
            // Minimal inline math to avoid further dependencies here; we will replace with GLM later if needed
            // Construct a right-handed perspective projection
            const float fov = 45.0F;
            const float radians = fov * 3.1415926535F / 180.0F;
            const float aspect = static_cast<float>(impl->swapchain_extent.width) / static_cast<float>(impl->swapchain_extent.height == 0 ? 1 : impl->swapchain_extent.height);
            const float znear = 0.1F;
            const float zfar = 100.0F;
            const float f = 1.0F / std::tanf(radians * 0.5F);
            float proj[16] = {0};
            proj[0] = f / aspect;
            proj[5] = f;
            proj[10] = (zfar + znear) / (znear - zfar);
            proj[11] = -1.0F;
            proj[14] = (2.0F * zfar * znear) / (znear - zfar);

            // Simple look-at view from (0,0,3) to origin with up (0,1,0)
            const float eye[3] = {0.0F, 0.0F, 3.0F};
            const float center[3] = {0.0F, 0.0F, 0.0F};
            const float up[3] = {0.0F, 1.0F, 0.0F};
            auto norm3 = [](float v[3]) noexcept {
                const float len = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
                if (len > 0.0F) { v[0]/=len; v[1]/=len; v[2]/=len; }
            };
            float fwd[3] = {center[0]-eye[0], center[1]-eye[1], center[2]-eye[2]};
            norm3(fwd);
            float s[3] = {fwd[1]*up[2] - fwd[2]*up[1], fwd[2]*up[0] - fwd[0]*up[2], fwd[0]*up[1] - fwd[1]*up[0]};
            norm3(s);
            float u[3] = {s[1]*fwd[2] - s[2]*fwd[1], s[2]*fwd[0] - s[0]*fwd[2], s[0]*fwd[1] - s[1]*fwd[0]};

            float view[16] = {0};
            view[0] = s[0]; view[4] = s[1]; view[8] = s[2];
            view[1] = u[0]; view[5] = u[1]; view[9] = u[2];
            view[2] = -fwd[0]; view[6] = -fwd[1]; view[10] = -fwd[2];
            view[15] = 1.0F;
            // Translation
            view[12] = -(s[0]*eye[0] + s[1]*eye[1] + s[2]*eye[2]);
            view[13] = -(u[0]*eye[0] + u[1]*eye[1] + u[2]*eye[2]);
            view[14] = -(-fwd[0]*eye[0] + -fwd[1]*eye[1] + -fwd[2]*eye[2]);

            // Model = identity
            float model[16] = {0};
            model[0]=1.0F; model[5]=1.0F; model[10]=1.0F; model[15]=1.0F;

            // mvp = proj * view * model
            auto matmul = [](const float a[16], const float b[16], float out[16]) noexcept {
                for (int r=0;r<4;++r) { for (int c=0;c<4;++c) { out[r*4+c] = a[r*4+0]*b[0*4+c] + a[r*4+1]*b[1*4+c] + a[r*4+2]*b[2*4+c] + a[r*4+3]*b[3*4+c]; } }
            };
            float pv[16] = {0}; float mvp[16] = {0};
            matmul(proj, view, pv);
            matmul(pv, model, mvp);
            vkCmdPushConstants(cmd, impl->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0U, static_cast<uint32_t>(sizeof(float)*16U), mvp);
        }
        if (impl->pipeline != VK_NULL_HANDLE && impl->vertex_buffer != VK_NULL_HANDLE && impl->index_buffer != VK_NULL_HANDLE && impl->index_count > 0U) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, impl->pipeline);
            VkDeviceSize offs[] = {0};
            VkBuffer vbs[] = {impl->vertex_buffer};
            vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offs);
            vkCmdBindIndexBuffer(cmd, impl->index_buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, impl->index_count, 1, 0, 0, 0);
        }
        if (impl->ui_renderer) {
            impl->ui_renderer(static_cast<void*>(cmd));
        }
        vkCmdEndRenderPass(cmd);
        vkEndCommandBuffer(cmd);
    }

    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo si {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1U;
    si.pWaitSemaphores = &impl->image_available;
    si.pWaitDstStageMask = wait_stages;
    si.commandBufferCount = 1U;
    VkCommandBuffer cmd_submit = impl->command_buffers[image_index];
    si.pCommandBuffers = &cmd_submit;
    si.signalSemaphoreCount = 1U;
    si.pSignalSemaphores = &impl->render_finished;
    vkQueueSubmit(impl->graphics_queue, 1U, &si, impl->in_flight);

    VkPresentInfoKHR pi {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1U;
    pi.pWaitSemaphores = &impl->render_finished;
    VkSwapchainKHR sc = impl->swapchain;
    pi.swapchainCount = 1U;
    pi.pSwapchains = &sc;
    pi.pImageIndices = &image_index;
    vkQueuePresentKHR(impl->graphics_queue, &pi);
}

void VulkanContext::set_ui_renderer(const std::function<void(void* cmd_buffer)>& renderer) noexcept {
    impl->ui_renderer = renderer;
}

void VulkanContext::set_subdivisions(std::uint32_t subdivisions) noexcept {
    if (impl == nullptr) {
        return;
    }
    if (impl->current_subdivisions == subdivisions) {
        return;
    }
    impl->current_subdivisions = subdivisions;
    try {
        IcosphereBuilder builder {};
        auto mesh = builder.build(subdivisions);
        if (mesh) {
            upload_mesh_to_gpu(*this, *mesh);
        }
        if (impl->pipeline == VK_NULL_HANDLE) {
            create_pipeline(*this);
        }
    } catch (...) {
        // ignore and keep previous state
    }
}

} // namespace vulkano

#else

namespace vulkano {

class Window;

VulkanContext::VulkanContext(const Window&) : impl {nullptr} {
}

VulkanContext::~VulkanContext() = default;
VulkanContext::VulkanContext(VulkanContext&&) noexcept = default;
VulkanContext& VulkanContext::operator=(VulkanContext&&) noexcept = default;
void VulkanContext::wait_idle() const noexcept {
}
void VulkanContext::draw_frame() {
}

void VulkanContext::set_subdivisions(std::uint32_t) noexcept {
}

} // namespace vulkano

#endif
