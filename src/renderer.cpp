#include <cstring>
#include <stdexcept>
#include <vector>

#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#include <vulkan_app/renderer.hpp>
#include <vulkan_app/icosphere.hpp>

namespace vulkan_app {

namespace {
#ifndef NDEBUG
constexpr bool kEnableValidation{true};
#else
constexpr bool kEnableValidation{false};
#endif

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* /*user_data*/) {
    (void)type;
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        fprintf(stderr, "Vulkan: %s\n", callback_data->pMessage);
    }
    return VK_FALSE;
}

VkFormat find_depth_format(VkPhysicalDevice gpu) {
    const VkFormat candidates[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT};
    for (VkFormat fmt : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(gpu, fmt, &props);
        if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0U) {
            return fmt;
        }
    }
    return VK_FORMAT_D32_SFLOAT;
}

std::uint32_t find_memory_type(VkPhysicalDevice gpu, std::uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_props{};
    vkGetPhysicalDeviceMemoryProperties(gpu, &mem_props);
    for (std::uint32_t i{0U}; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1U << i)) != 0U && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error{"No suitable memory type"};
}

void create_buffer(VkPhysicalDevice gpu, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage,
                   VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &memory) {
    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &bci, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create buffer"};
    }
    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device, buffer, &req);
    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = find_memory_type(gpu, req.memoryTypeBits, properties);
    if (vkAllocateMemory(device, &mai, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to allocate buffer memory"};
    }
    vkBindBufferMemory(device, buffer, memory, 0);
}

void create_image(VkPhysicalDevice gpu, VkDevice device, std::uint32_t w, std::uint32_t h, VkFormat format,
                  VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                  VkImage &image, VkDeviceMemory &memory) {
    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.extent = {w, h, 1U};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.format = format;
    ici.tiling = tiling;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici.usage = usage;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(device, &ici, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create image"};
    }
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(device, image, &req);
    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = find_memory_type(gpu, req.memoryTypeBits, properties);
    if (vkAllocateMemory(device, &mai, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to allocate image memory"};
    }
    vkBindImageMemory(device, image, memory, 0);
}

VkImageView create_image_view(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect) {
    VkImageViewCreateInfo ivci{};
    ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.image = image;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = format;
    ivci.subresourceRange.aspectMask = aspect;
    ivci.subresourceRange.baseMipLevel = 0;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.baseArrayLayer = 0;
    ivci.subresourceRange.layerCount = 1;
    VkImageView view{};
    if (vkCreateImageView(device, &ivci, nullptr, &view) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create image view"};
    }
    return view;
}

std::vector<char> read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f == nullptr) {
        throw std::runtime_error{"Failed to open file"};
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<char> data(static_cast<std::size_t>(sz));
    fread(data.data(), 1, static_cast<std::size_t>(sz), f);
    fclose(f);
    return data;
}

VkShaderModule create_shader_module(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo smci{};
    smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.codeSize = code.size();
    smci.pCode = reinterpret_cast<const std::uint32_t*>(code.data());
    VkShaderModule module{};
    if (vkCreateShaderModule(device, &smci, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create shader module"};
    }
    return module;
}

} // namespace

Renderer::Renderer(GLFWwindow* window) {
    init_instance();
    init_surface(window);
    pick_physical_device();
    create_device();
    create_swapchain();
    create_render_pass();
    create_depth_resources();
    create_framebuffers();
    create_descriptor_layouts();
    create_pipeline();
    create_command_pool_and_buffers();
    create_sync_objects();
    create_sampler();
    create_textures_if_needed();
}

Renderer::~Renderer() noexcept {
    destroy_swapchain();
    if (sampler_ != VK_NULL_HANDLE) { vkDestroySampler(device_, sampler_, nullptr); }
    if (albedo_view_ != VK_NULL_HANDLE) { vkDestroyImageView(device_, albedo_view_, nullptr); }
    if (albedo_image_ != VK_NULL_HANDLE) { vkDestroyImage(device_, albedo_image_, nullptr); }
    if (albedo_memory_ != VK_NULL_HANDLE) { vkFreeMemory(device_, albedo_memory_, nullptr); }
    if (normal_view_ != VK_NULL_HANDLE) { vkDestroyImageView(device_, normal_view_, nullptr); }
    if (normal_image_ != VK_NULL_HANDLE) { vkDestroyImage(device_, normal_image_, nullptr); }
    if (normal_memory_ != VK_NULL_HANDLE) { vkFreeMemory(device_, normal_memory_, nullptr); }
    if (index_buffer_ != VK_NULL_HANDLE) { vkDestroyBuffer(device_, index_buffer_, nullptr); }
    if (index_memory_ != VK_NULL_HANDLE) { vkFreeMemory(device_, index_memory_, nullptr); }
    if (vertex_buffer_ != VK_NULL_HANDLE) { vkDestroyBuffer(device_, vertex_buffer_, nullptr); }
    if (vertex_memory_ != VK_NULL_HANDLE) { vkFreeMemory(device_, vertex_memory_, nullptr); }
    if (desc_pool_ != VK_NULL_HANDLE) { vkDestroyDescriptorPool(device_, desc_pool_, nullptr); }
    if (desc_set_layout_ != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device_, desc_set_layout_, nullptr); }
    if (pipeline_ != VK_NULL_HANDLE) { vkDestroyPipeline(device_, pipeline_, nullptr); }
    if (pipeline_layout_ != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr); }
    if (render_pass_ != VK_NULL_HANDLE) { vkDestroyRenderPass(device_, render_pass_, nullptr); }
    if (cmd_pool_ != VK_NULL_HANDLE) { vkDestroyCommandPool(device_, cmd_pool_, nullptr); }
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        vkDestroyDevice(device_, nullptr);
    }
    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
    }
    if (debug_messenger_ != VK_NULL_HANDLE) {
        auto destroyFn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroyFn != nullptr) {
            destroyFn(instance_, debug_messenger_, nullptr);
        }
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
    }
}

void Renderer::init_instance() {
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "vulkan_app";
    app_info.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    app_info.pEngineName = "vulkan_app";
    app_info.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    std::uint32_t glfw_count{0U};
    const char** glfw_ext = glfwGetRequiredInstanceExtensions(&glfw_count);
    std::vector<const char*> extensions(glfw_ext, glfw_ext + glfw_count);
    if (kEnableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    const std::vector<const char*> layers{
        "VK_LAYER_KHRONOS_validation"
    };

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app_info;
    ci.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();
    if (kEnableValidation) {
        ci.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
        ci.ppEnabledLayerNames = layers.data();
    }

    VkDebugUtilsMessengerCreateInfoEXT debug_ci{};
    if (kEnableValidation) {
        debug_ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_ci.pfnUserCallback = debug_callback;
        ci.pNext = &debug_ci;
    }

    if (vkCreateInstance(&ci, nullptr, &instance_) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create Vulkan instance"};
    }

    if (kEnableValidation) {
        auto createFn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
        if (createFn != nullptr) {
            if (createFn(instance_, &debug_ci, nullptr, &debug_messenger_) != VK_SUCCESS) {
                fprintf(stderr, "Failed to set up debug messenger\n");
            }
        }
    }
}

void Renderer::init_surface(GLFWwindow* window) {
    if (glfwCreateWindowSurface(instance_, window, nullptr, &surface_) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create window surface"};
    }
}

void Renderer::pick_physical_device() {
    std::uint32_t count{0U};
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0U) {
        throw std::runtime_error{"No Vulkan-capable GPUs found"};
    }
    std::vector<VkPhysicalDevice> gpus(count);
    vkEnumeratePhysicalDevices(instance_, &count, gpus.data());
    physical_device_ = gpus[0];
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physical_device_, &props);
    device_name_ = props.deviceName;
}

void Renderer::create_device() {
    // Query queue families
    std::uint32_t family_count{0U};
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &family_count, families.data());

    // Naive selection: first with graphics; assume present is same.
    for (std::uint32_t i{0U}; i < family_count; ++i) {
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
            graphics_family_ = i;
            present_family_ = i;
            break;
        }
    }
    const float prio{1.0F};
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = graphics_family_;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;

    VkPhysicalDeviceFeatures features{};

    const char* device_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.pQueueCreateInfos = &qci;
    dci.queueCreateInfoCount = 1;
    dci.pEnabledFeatures = &features;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = device_exts;
    if (vkCreateDevice(physical_device_, &dci, nullptr, &device_) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create logical device"};
    }
    vkGetDeviceQueue(device_, graphics_family_, 0, &graphics_queue_);
    present_queue_ = graphics_queue_;
}

void Renderer::create_swapchain() {
    // Query surface capabilities
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &caps);
    std::uint32_t format_count{0U};
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, formats.data());
    std::uint32_t present_count{0U};
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &present_count, nullptr);
    std::vector<VkPresentModeKHR> presents(present_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &present_count, presents.data());

    VkSurfaceFormatKHR chosen_format = formats[0];
    for (const auto &f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen_format = f;
            break;
        }
    }
    VkPresentModeKHR chosen_present = VK_PRESENT_MODE_FIFO_KHR;
    for (auto pm : presents) {
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR) { chosen_present = pm; break; }
    }
    VkExtent2D extent{};
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        extent = {800U, 600U};
        extent.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, extent.width));
        extent.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, extent.height));
    }
    std::uint32_t image_count = caps.minImageCount + 1U;
    if (caps.maxImageCount > 0U && image_count > caps.maxImageCount) {
        image_count = caps.maxImageCount;
    }
    VkSwapchainCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = surface_;
    sci.minImageCount = image_count;
    sci.imageFormat = chosen_format.format;
    sci.imageColorSpace = chosen_format.colorSpace;
    sci.imageExtent = extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = chosen_present;
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(device_, &sci, nullptr, &swapchain_) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create swapchain"};
    }
    swapchain_format_ = chosen_format.format;
    swapchain_extent_ = extent;
    std::uint32_t count{0U};
    vkGetSwapchainImagesKHR(device_, swapchain_, &count, nullptr);
    swapchain_images_.resize(count);
    vkGetSwapchainImagesKHR(device_, swapchain_, &count, swapchain_images_.data());
    swapchain_image_views_.resize(count);
    for (std::size_t i{0}; i < swapchain_images_.size(); ++i) {
        swapchain_image_views_[i] = create_image_view(device_, swapchain_images_[i], swapchain_format_, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void Renderer::destroy_swapchain() {
    for (auto fb : framebuffers_) { vkDestroyFramebuffer(device_, fb, nullptr); }
    framebuffers_.clear();
    if (depth_view_ != VK_NULL_HANDLE) { vkDestroyImageView(device_, depth_view_, nullptr); depth_view_ = VK_NULL_HANDLE; }
    if (depth_image_ != VK_NULL_HANDLE) { vkDestroyImage(device_, depth_image_, nullptr); depth_image_ = VK_NULL_HANDLE; }
    if (depth_memory_ != VK_NULL_HANDLE) { vkFreeMemory(device_, depth_memory_, nullptr); depth_memory_ = VK_NULL_HANDLE; }
    for (auto iv : swapchain_image_views_) { vkDestroyImageView(device_, iv, nullptr); }
    swapchain_image_views_.clear();
    if (render_pass_ != VK_NULL_HANDLE) { vkDestroyRenderPass(device_, render_pass_, nullptr); render_pass_ = VK_NULL_HANDLE; }
    if (pipeline_ != VK_NULL_HANDLE) { vkDestroyPipeline(device_, pipeline_, nullptr); pipeline_ = VK_NULL_HANDLE; }
    if (pipeline_layout_ != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr); pipeline_layout_ = VK_NULL_HANDLE; }
    if (device_ != VK_NULL_HANDLE && swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

void Renderer::resize() {
    recreate_swapchain();
}

void Renderer::update_mesh(const Mesh& mesh) {
    index_count_ = static_cast<std::uint32_t>(mesh.indices.size());
    VkDeviceSize vb_size = static_cast<VkDeviceSize>(mesh.vertices.size() * sizeof(Vertex));
    VkDeviceSize ib_size = static_cast<VkDeviceSize>(mesh.indices.size() * sizeof(std::uint32_t));
    if (vertex_buffer_ == VK_NULL_HANDLE) {
        create_buffer(physical_device_, device_, vb_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      vertex_buffer_, vertex_memory_);
    }
    if (index_buffer_ == VK_NULL_HANDLE) {
        create_buffer(physical_device_, device_, ib_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      index_buffer_, index_memory_);
    }
    void* data{nullptr};
    vkMapMemory(device_, vertex_memory_, 0, vb_size, 0, &data);
    std::memcpy(data, mesh.vertices.data(), static_cast<std::size_t>(vb_size));
    vkUnmapMemory(device_, vertex_memory_);
    vkMapMemory(device_, index_memory_, 0, ib_size, 0, &data);
    std::memcpy(data, mesh.indices.data(), static_cast<std::size_t>(ib_size));
    vkUnmapMemory(device_, index_memory_);
}

void Renderer::draw_frame(const CameraUBO& /*camera*/, const Light& /*light*/, const Material& /*material*/, const SsaoParams& /*ssao*/) {
    vkWaitForFences(device_, 1, &in_flight_[current_frame_], VK_TRUE, UINT64_MAX);
    std::uint32_t image_index{0U};
    VkResult acq = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, image_available_[current_frame_], VK_NULL_HANDLE, &image_index);
    if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return;
    }
    vkResetFences(device_, 1, &in_flight_[current_frame_]);
    VkCommandBuffer cmd = cmd_buffers_[image_index];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &bi);

    VkClearValue clears[2];
    clears[0].color = {{0.05F, 0.05F, 0.08F, 1.0F}};
    clears[1].depthStencil = {1.0F, 0};
    VkRenderPassBeginInfo rbi{};
    rbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rbi.renderPass = render_pass_;
    rbi.framebuffer = framebuffers_[image_index];
    rbi.renderArea.offset = {0, 0};
    rbi.renderArea.extent = swapchain_extent_;
    rbi.clearValueCount = 2;
    rbi.pClearValues = clears;
    vkCmdBeginRenderPass(cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    VkViewport vp{}; vp.x = 0; vp.y = 0; vp.width = static_cast<float>(swapchain_extent_.width); vp.height = static_cast<float>(swapchain_extent_.height); vp.minDepth = 0.0F; vp.maxDepth = 1.0F;
    VkRect2D sc{}; sc.offset = {0,0}; sc.extent = swapchain_extent_;
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &sc);
    VkDeviceSize offs{0};
    if (vertex_buffer_ != VK_NULL_HANDLE) {
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer_, &offs);
    }
    if (index_buffer_ != VK_NULL_HANDLE) {
        vkCmdBindIndexBuffer(cmd, index_buffer_, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, index_count_, 1, 0, 0, 0);
    }
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkSemaphore wait_sems[] = { image_available_[current_frame_] };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signal_sems[] = { render_finished_[current_frame_] };
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = wait_sems;
    si.pWaitDstStageMask = wait_stages;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = signal_sems;
    if (vkQueueSubmit(graphics_queue_, 1, &si, in_flight_[current_frame_]) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to submit"};
    }
    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = signal_sems;
    VkSwapchainKHR swps[] = { swapchain_ };
    pi.swapchainCount = 1;
    pi.pSwapchains = swps;
    pi.pImageIndices = &image_index;
    VkResult pres = vkQueuePresentKHR(present_queue_, &pi);
    if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) {
        recreate_swapchain();
    }
    current_frame_ = (current_frame_ + 1U) % image_available_.size();
}

void Renderer::create_render_pass() {
    VkAttachmentDescription color{};
    color.format = swapchain_format_;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth{};
    depth.format = find_depth_format(physical_device_);
    depth_format_ = depth.format;
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference cref{}; cref.attachment = 0; cref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference dref{}; dref.attachment = 1; dref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &cref;
    sub.pDepthStencilAttachment = &dref;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription atts[] = { color, depth };
    VkRenderPassCreateInfo rpci{};
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 2;
    rpci.pAttachments = atts;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &sub;
    rpci.dependencyCount = 1;
    rpci.pDependencies = &dep;
    if (vkCreateRenderPass(device_, &rpci, nullptr, &render_pass_) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create render pass"};
    }
}

void Renderer::create_pipeline() {
    // Load shaders from build dir
    // Try both build and source as fallback
    std::string base = "./shaders";
    std::string build = "./build/shaders";
    std::vector<char> vs;
    std::vector<char> fs;
    try { vs = read_file((build + "/forward.vert.spv").c_str()); } catch (...) { vs = read_file((base + "/forward.vert.spv").c_str()); }
    try { fs = read_file((build + "/forward.frag.spv").c_str()); } catch (...) { fs = read_file((base + "/forward.frag.spv").c_str()); }
    VkShaderModule vsm = create_shader_module(device_, vs);
    VkShaderModule fsm = create_shader_module(device_, fs);
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vsm;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fsm;
    stages[1].pName = "main";

    VkVertexInputBindingDescription bind{};
    bind.binding = 0;
    bind.stride = sizeof(Vertex);
    bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attrs[5]{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)};
    attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)};
    attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)};
    attrs[3] = {3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, tangent)};
    attrs[4] = {4, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, bitangent)};
    VkPipelineVertexInputStateCreateInfo vis{};
    vis.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vis.vertexBindingDescriptionCount = 1;
    vis.pVertexBindingDescriptions = &bind;
    vis.vertexAttributeDescriptionCount = 5;
    vis.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ias{};
    ias.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ias.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport vp{}; vp.x = 0; vp.y = 0; vp.width = static_cast<float>(swapchain_extent_.width); vp.height = static_cast<float>(swapchain_extent_.height); vp.minDepth = 0.0F; vp.maxDepth = 1.0F;
    VkRect2D sc{}; sc.offset = {0,0}; sc.extent = swapchain_extent_;
    VkPipelineViewportStateCreateInfo vps{};
    vps.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vps.viewportCount = 1; vps.pViewports = &vp;
    vps.scissorCount = 1; vps.pScissors = &sc;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0F;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;

    VkPushConstantRange pcr{}; pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; pcr.offset = 0; pcr.size = sizeof(glm::mat4);
    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &desc_set_layout_;
    plci.pushConstantRangeCount = 1; plci.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(device_, &plci, nullptr, &pipeline_layout_) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create pipeline layout"};
    }
    VkGraphicsPipelineCreateInfo gp{};
    gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gp.stageCount = 2; gp.pStages = stages;
    gp.pVertexInputState = &vis;
    gp.pInputAssemblyState = &ias;
    gp.pViewportState = &vps;
    gp.pRasterizationState = &rs;
    gp.pMultisampleState = &ms;
    gp.pDepthStencilState = &ds;
    gp.pColorBlendState = &cb;
    gp.layout = pipeline_layout_;
    gp.renderPass = render_pass_;
    gp.subpass = 0;
    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &gp, nullptr, &pipeline_) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create graphics pipeline"};
    }
    vkDestroyShaderModule(device_, vsm, nullptr);
    vkDestroyShaderModule(device_, fsm, nullptr);
}

void Renderer::create_framebuffers() {
    framebuffers_.resize(swapchain_image_views_.size());
    for (std::size_t i{0}; i < swapchain_image_views_.size(); ++i) {
        VkImageView atts[2] = { swapchain_image_views_[i], depth_view_ };
        VkFramebufferCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass = render_pass_;
        fci.attachmentCount = 2;
        fci.pAttachments = atts;
        fci.width = swapchain_extent_.width;
        fci.height = swapchain_extent_.height;
        fci.layers = 1;
        if (vkCreateFramebuffer(device_, &fci, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            throw std::runtime_error{"Failed to create framebuffer"};
        }
    }
}

void Renderer::create_command_pool_and_buffers() {
    VkCommandPoolCreateInfo cp{};
    cp.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cp.queueFamilyIndex = graphics_family_;
    cp.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(device_, &cp, nullptr, &cmd_pool_) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create command pool"};
    }
    cmd_buffers_.resize(swapchain_images_.size());
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = cmd_pool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = static_cast<std::uint32_t>(cmd_buffers_.size());
    if (vkAllocateCommandBuffers(device_, &ai, cmd_buffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to allocate command buffers"};
    }
}

void Renderer::create_sync_objects() {
    const std::size_t frames = 2;
    image_available_.resize(frames);
    render_finished_.resize(frames);
    in_flight_.resize(frames);
    VkSemaphoreCreateInfo si{}; si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fi{}; fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO; fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (std::size_t i{0}; i < frames; ++i) {
        if (vkCreateSemaphore(device_, &si, nullptr, &image_available_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device_, &si, nullptr, &render_finished_[i]) != VK_SUCCESS ||
            vkCreateFence(device_, &fi, nullptr, &in_flight_[i]) != VK_SUCCESS) {
            throw std::runtime_error{"Failed to create sync objects"};
        }
    }
}

void Renderer::create_descriptor_layouts() {
    VkDescriptorSetLayoutBinding b0{}; b0.binding = 0; b0.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; b0.descriptorCount = 1; b0.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutBinding b3{}; b3.binding = 3; b3.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; b3.descriptorCount = 1; b3.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutBinding b4{}; b4.binding = 4; b4.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; b4.descriptorCount = 1; b4.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutBinding bindings[] = { b0, b3, b4 };
    VkDescriptorSetLayoutCreateInfo lci{};
    lci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    lci.bindingCount = 3;
    lci.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(device_, &lci, nullptr, &desc_set_layout_) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create descriptor set layout"};
    }
}

void Renderer::create_descriptor_pool_and_sets() {
    // Create UBOs
    VkDeviceSize cam_size = sizeof(CameraUBO);
    VkDeviceSize mat_size = sizeof(Material);
    VkDeviceSize light_size = sizeof(Light);
    create_buffer(physical_device_, device_, cam_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  camera_buffer_, camera_memory_);
    create_buffer(physical_device_, device_, mat_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  material_buffer_, material_memory_);
    create_buffer(physical_device_, device_, light_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  light_buffer_, light_memory_);

    // Descriptor pool and sets
    VkDescriptorPoolSize sizes[3]{};
    sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; sizes[0].descriptorCount = static_cast<std::uint32_t>(swapchain_images_.size());
    sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; sizes[1].descriptorCount = static_cast<std::uint32_t>(swapchain_images_.size());
    sizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; sizes[2].descriptorCount = static_cast<std::uint32_t>(swapchain_images_.size());
    VkDescriptorPoolCreateInfo dpci{};
    dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.poolSizeCount = 3; dpci.pPoolSizes = sizes;
    dpci.maxSets = static_cast<std::uint32_t>(swapchain_images_.size());
    if (vkCreateDescriptorPool(device_, &dpci, nullptr, &desc_pool_) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create descriptor pool"};
    }
    std::vector<VkDescriptorSetLayout> layouts(swapchain_images_.size(), desc_set_layout_);
    VkDescriptorSetAllocateInfo dsai{};
    dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool = desc_pool_;
    dsai.descriptorSetCount = static_cast<std::uint32_t>(layouts.size());
    dsai.pSetLayouts = layouts.data();
    desc_sets_.resize(layouts.size());
    if (vkAllocateDescriptorSets(device_, &dsai, desc_sets_.data()) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to allocate descriptor sets"};
    }
    for (std::size_t i{0}; i < desc_sets_.size(); ++i) {
        VkDescriptorBufferInfo cam{}; cam.buffer = camera_buffer_; cam.offset = 0; cam.range = cam_size;
        VkDescriptorBufferInfo mat{}; mat.buffer = material_buffer_; mat.offset = 0; mat.range = mat_size;
        VkDescriptorBufferInfo lig{}; lig.buffer = light_buffer_; lig.offset = 0; lig.range = light_size;
        VkWriteDescriptorSet writes[3]{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; writes[0].dstSet = desc_sets_[i]; writes[0].dstBinding = 0; writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; writes[0].descriptorCount = 1; writes[0].pBufferInfo = &cam;
        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; writes[1].dstSet = desc_sets_[i]; writes[1].dstBinding = 3; writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; writes[1].descriptorCount = 1; writes[1].pBufferInfo = &mat;
        writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; writes[2].dstSet = desc_sets_[i]; writes[2].dstBinding = 4; writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; writes[2].descriptorCount = 1; writes[2].pBufferInfo = &lig;
        vkUpdateDescriptorSets(device_, 3, writes, 0, nullptr);
    }
}

void Renderer::create_depth_resources() {
    create_image(physical_device_, device_, swapchain_extent_.width, swapchain_extent_.height, depth_format_,
                 VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 depth_image_, depth_memory_);
    depth_view_ = create_image_view(device_, depth_image_, depth_format_, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void Renderer::create_sampler() {
    VkSamplerCreateInfo sci{}; sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter = VK_FILTER_LINEAR; sci.minFilter = VK_FILTER_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.anisotropyEnable = VK_FALSE; sci.maxAnisotropy = 1.0F;
    sci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sci.unnormalizedCoordinates = VK_FALSE;
    if (vkCreateSampler(device_, &sci, nullptr, &sampler_) != VK_SUCCESS) {
        throw std::runtime_error{"Failed to create sampler"};
    }
}

void Renderer::create_textures_if_needed() {
    // TODO: Upload procedural checkerboard textures (omitted in minimal phase)
}

void Renderer::recreate_swapchain() {
    vkDeviceWaitIdle(device_);
    destroy_swapchain();
    create_swapchain();
    create_render_pass();
    create_depth_resources();
    create_framebuffers();
    create_pipeline();
}

} // namespace vulkan_app
