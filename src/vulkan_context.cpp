#include <vulkano/vulkan_context.hpp>
#include <vulkano/window.hpp>

#if VULKANO_HAS_VULKAN
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <array>
#include <vector>
#include <stdexcept>

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

    Impl() = default;
    ~Impl() = default;
};

struct VulkanContextAccess final {
    static VulkanContext::Impl& get(VulkanContext& ctx) noexcept { return *ctx.impl; }
};

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
    std::array<const char*, 4> extensions {};
    for (uint32_t i = 0; i < ext_count; ++i) {
        extensions[i] = exts[i];
    }

    VkInstanceCreateInfo ci {};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app_info;
    ci.enabledExtensionCount = ext_count;
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
    auto& impl = VulkanContextAccess::get(ctx);
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
    auto& impl = VulkanContextAccess::get(ctx);
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
    auto& impl = VulkanContextAccess::get(ctx);
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
    auto& impl = VulkanContextAccess::get(ctx);
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
        vkCmdEndRenderPass(impl.command_buffers[i]);
        vkEndCommandBuffer(impl.command_buffers[i]);
    }
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

    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo si {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1U;
    si.pWaitSemaphores = &impl->image_available;
    si.pWaitDstStageMask = wait_stages;
    si.commandBufferCount = 1U;
    VkCommandBuffer cmd = impl->command_buffers[image_index];
    si.pCommandBuffers = &cmd;
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

} // namespace vulkano

#endif
