#include "vulkan_app.hpp"

#include <stdexcept>
#include <set>
#include <cstring>
#include <array>
#include <fstream>
#include <algorithm>
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

#ifdef ENABLE_VALIDATION
static const bool kEnableValidation = true;
#else
static const bool kEnableValidation = false;
#endif

namespace {
    // Constants
    const std::vector<const char*> kValidationLayers{ "VK_LAYER_KHRONOS_validation" };

    // Descriptor pool sizes for ImGui
    const std::array<VkDescriptorPoolSize, 11> kImGuiPoolSizes = { {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    } };

    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {
        (void)messageType; (void)pUserData;
        if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            fprintf(stderr, "Validation: %s\n", pCallbackData->pMessage);
        }
        return VK_FALSE;
    }

    std::vector<uint32_t> readSpv(const std::string& path) {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file) { throw std::runtime_error("Failed to open SPV file: " + path); }
        auto size = static_cast<size_t>(file.tellg());
        std::vector<uint32_t> buffer(size / sizeof(uint32_t));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        return buffer;
    }

    uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t type_filter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties mem_props{};
        vkGetPhysicalDeviceMemoryProperties(phys, &mem_props);
        for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
            if ((type_filter & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        throw std::runtime_error("Suitable memory type not found");
    }

    void setDebugName(VkDevice device, VkObjectType type, uint64_t handle, const char* name) {
        if (!kEnableValidation) { return; }
        auto fn = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT");
        if (!fn) { return; }
        VkDebugUtilsObjectNameInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        info.objectType = type;
        info.objectHandle = handle;
        info.pObjectName = name;
        fn(device, &info);
    }

    void cmdBeginLabel(VkDevice device, VkCommandBuffer cmd, const char* name, const float color[4]) {
        if (!kEnableValidation) { return; }
        auto fn = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT");
        if (!fn) { return; }
        VkDebugUtilsLabelEXT label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
        label.pLabelName = name;
        if (color) { std::copy(color, color + 4, label.color); }
        fn(cmd, &label);
    }

    void cmdEndLabel(VkDevice device, VkCommandBuffer cmd) {
        if (!kEnableValidation) { return; }
        auto fn = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT");
        if (!fn) { return; }
        fn(cmd);
    }
}

VkVertexInputBindingDescription Vertex::bindingDescription() {
    VkVertexInputBindingDescription desc{};
    desc.binding = 0;
    desc.stride = sizeof(Vertex);
    desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return desc;
}

std::array<VkVertexInputAttributeDescription, 1> Vertex::attributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 1> attrs{};
    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[0].offset = 0;
    return attrs;
}

VulkanApp::VulkanApp(GLFWwindow* window) : window_{window} {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createDevice();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createDescriptorPool();
    createPipeline();
    createFramebuffers();
    createCommandPool();
    createVertexBuffer();
    createCommandBuffers();
    createSyncObjects();
}

VulkanApp::~VulkanApp() {
    vkDeviceWaitIdle(device_);

    vkDestroyBuffer(device_, vertex_buffer_, nullptr);
    vkFreeMemory(device_, vertex_buffer_memory_, nullptr);

    for (auto s : render_finished_semaphores_) { vkDestroySemaphore(device_, s, nullptr); }
    for (auto s : render_finished_image_semaphores_) { vkDestroySemaphore(device_, s, nullptr); }
    for (auto s : image_available_semaphores_) { vkDestroySemaphore(device_, s, nullptr); }
    for (auto f : in_flight_fences_) { vkDestroyFence(device_, f, nullptr); }

    vkDestroyDescriptorPool(device_, imgui_descriptor_pool_, nullptr);

    vkDestroyPipeline(device_, graphics_pipeline_, nullptr);
    vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
    vkDestroyRenderPass(device_, render_pass_, nullptr);

    for (auto fb : swapchain_framebuffers_) { vkDestroyFramebuffer(device_, fb, nullptr); }
    for (auto iv : swapchain_image_views_) { vkDestroyImageView(device_, iv, nullptr); }
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);

    vkDestroyCommandPool(device_, command_pool_, nullptr);

    vkDestroyDevice(device_, nullptr);
    vkDestroySurfaceKHR(instance_, surface_, nullptr);

    if (kEnableValidation && debug_messenger_ != VK_NULL_HANDLE) {
        auto destroy = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT");
        if (destroy) { destroy(instance_, debug_messenger_, nullptr); }
    }
    vkDestroyInstance(instance_, nullptr);
}

void VulkanApp::waitIdle() const { vkDeviceWaitIdle(device_); }

void VulkanApp::createInstance() {
    if (kEnableValidation) {
        uint32_t count = 0;
        vkEnumerateInstanceLayerProperties(&count, nullptr);
        std::vector<VkLayerProperties> layers(count);
        vkEnumerateInstanceLayerProperties(&count, layers.data());
        for (const char* name : kValidationLayers) {
            bool found = false;
            for (auto& l : layers) { if (std::strcmp(l.layerName, name) == 0) { found = true; break; } }
            if (!found) { throw std::runtime_error("Validation layer not found"); }
        }
    }

    VkApplicationInfo app_info{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app_info.pApplicationName = "Vulkan Triangle";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Codex";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    uint32_t glfw_ext_count = 0;
    const char** glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
    std::vector<const char*> extensions(glfw_exts, glfw_exts + glfw_ext_count);
    // For portability (MoltenVK) on macOS
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkInstanceCreateInfo create_info{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    VkDebugUtilsMessengerCreateInfoEXT dbg{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    if (kEnableValidation) {
        create_info.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
        create_info.ppEnabledLayerNames = kValidationLayers.data();

        dbg.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbg.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbg.pfnUserCallback = debugCallback;
        create_info.pNext = &dbg;
    }

    if (vkCreateInstance(&create_info, nullptr, &instance_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create instance");
    }
}

void VulkanApp::setupDebugMessenger() {
    if (!kEnableValidation) { return; }
    VkDebugUtilsMessengerCreateInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = debugCallback;
    auto create = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT");
    if (create) {
        if (create(instance_, &info, nullptr, &debug_messenger_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create debug messenger");
        }
    }
}

void VulkanApp::createSurface() {
    if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }
}

QueueFamilies VulkanApp::findQueueFamilies(VkPhysicalDevice dev) const {
    QueueFamilies q{};
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, families.data());
    for (uint32_t i = 0; i < count; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { q.graphics = i; }
        VkBool32 present_supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface_, &present_supported);
        if (present_supported) { q.present = i; }
        if (q.complete()) { break; }
    }
    return q;
}

SwapchainSupport VulkanApp::querySwapchainSupport(VkPhysicalDevice dev) const {
    SwapchainSupport s{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface_, &s.capabilities);
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface_, &count, nullptr);
    s.formats.resize(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface_, &count, s.formats.data());
    count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface_, &count, nullptr);
    s.presentModes.resize(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface_, &count, s.presentModes.data());
    return s;
}

void VulkanApp::pickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) { throw std::runtime_error("No Vulkan physical devices found"); }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    // Prefer discrete GPU
    VkPhysicalDevice selected = VK_NULL_HANDLE;
    for (auto dev : devices) {
        QueueFamilies q = findQueueFamilies(dev);
        if (!q.complete()) { continue; }
        SwapchainSupport s = querySwapchainSupport(dev);
        if (s.formats.empty() || s.presentModes.empty()) { continue; }
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(dev, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) { selected = dev; break; }
        if (selected == VK_NULL_HANDLE) { selected = dev; }
    }
    if (selected == VK_NULL_HANDLE) { throw std::runtime_error("No suitable device found"); }
    physical_device_ = selected;
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physical_device_, &props);
    device_name_ = props.deviceName;
}

void VulkanApp::createDevice() {
    queue_family_indices_ = findQueueFamilies(physical_device_);
    std::set<uint32_t> unique_indices{ queue_family_indices_.graphics.value(), queue_family_indices_.present.value() };
    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queue_infos{};
    for (uint32_t index : unique_indices) {
        VkDeviceQueueCreateInfo q{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        q.queueFamilyIndex = index;
        q.queueCount = 1;
        q.pQueuePriorities = &priority;
        queue_infos.push_back(q);
    }

    VkPhysicalDeviceFeatures features{};
    std::vector<const char*> extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
    extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#else
    extensions.push_back("VK_KHR_portability_subset");
#endif

    VkDeviceCreateInfo info{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    info.queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size());
    info.pQueueCreateInfos = queue_infos.data();
    info.pEnabledFeatures = &features;
    info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();
    if (kEnableValidation) {
        info.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
        info.ppEnabledLayerNames = kValidationLayers.data();
    }

    if (vkCreateDevice(physical_device_, &info, nullptr, &device_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create device");
    }
    vkGetDeviceQueue(device_, queue_family_indices_.graphics.value(), 0, &graphics_queue_);
    vkGetDeviceQueue(device_, queue_family_indices_.present.value(), 0, &present_queue_);
}

VkSurfaceFormatKHR VulkanApp::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const {
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    return formats.front();
}

VkPresentModeKHR VulkanApp::choosePresentMode(const std::vector<VkPresentModeKHR>& modes) const {
    for (auto m : modes) { if (m == VK_PRESENT_MODE_MAILBOX_KHR) { return m; } }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanApp::chooseExtent(const VkSurfaceCapabilitiesKHR& caps) const {
    if (caps.currentExtent.width != UINT32_MAX) { return caps.currentExtent; }
    int w = 0, h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    VkExtent2D e{};
    e.width = std::clamp<uint32_t>(w, caps.minImageExtent.width, caps.maxImageExtent.width);
    e.height = std::clamp<uint32_t>(h, caps.minImageExtent.height, caps.maxImageExtent.height);
    return e;
}

void VulkanApp::createSwapchain() {
    auto support = querySwapchainSupport(physical_device_);
    auto surface_format = chooseSwapSurfaceFormat(support.formats);
    auto present_mode = choosePresentMode(support.presentModes);
    auto extent = chooseExtent(support.capabilities);

    uint32_t image_count = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && image_count > support.capabilities.maxImageCount) {
        image_count = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR info{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    info.surface = surface_;
    info.minImageCount = image_count;
    info.imageFormat = surface_format.format;
    info.imageColorSpace = surface_format.colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t indices[] = { queue_family_indices_.graphics.value(), queue_family_indices_.present.value() };
    if (queue_family_indices_.graphics != queue_family_indices_.present) {
        info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices = indices;
    } else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    info.preTransform = support.capabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = present_mode;
    info.clipped = VK_TRUE;
    info.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device_, &info, nullptr, &swapchain_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain");
    }
    setDebugName(device_, VK_OBJECT_TYPE_SWAPCHAIN_KHR, (uint64_t)swapchain_, "Swapchain");

    uint32_t actual_count = 0;
    vkGetSwapchainImagesKHR(device_, swapchain_, &actual_count, nullptr);
    swapchain_images_.resize(actual_count);
    vkGetSwapchainImagesKHR(device_, swapchain_, &actual_count, swapchain_images_.data());
    swapchain_image_format_ = surface_format.format;
    swapchain_extent_ = extent;
}

void VulkanApp::createImageViews() {
    swapchain_image_views_.resize(swapchain_images_.size());
    for (size_t i = 0; i < swapchain_images_.size(); ++i) {
        VkImageViewCreateInfo info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        info.image = swapchain_images_[i];
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = swapchain_image_format_;
        info.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device_, &info, nullptr, &swapchain_image_views_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view");
        }
        char name[64]{};
        std::snprintf(name, sizeof(name), "SwapchainImageView %zu", i);
        setDebugName(device_, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)swapchain_image_views_[i], name);
    }
}

void VulkanApp::createRenderPass() {
    VkAttachmentDescription color{};
    color.format = swapchain_image_format_;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    info.attachmentCount = 1;
    info.pAttachments = &color;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dep;

    if (vkCreateRenderPass(device_, &info, nullptr, &render_pass_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }
    setDebugName(device_, VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)render_pass_, "RenderPass");
}

VkShaderModule VulkanApp::createShaderModule(const std::vector<uint32_t>& code) const {
    VkShaderModuleCreateInfo info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    info.codeSize = code.size() * sizeof(uint32_t);
    info.pCode = code.data();
    VkShaderModule module{};
    if (vkCreateShaderModule(device_, &info, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }
    return module;
}

void VulkanApp::createPipeline() {
    std::string vert_path = std::string(SPIRV_DIR) + "/triangle.vert.spv";
    std::string frag_path = std::string(SPIRV_DIR) + "/triangle.frag.spv";
    auto vert_code = readSpv(vert_path);
    auto frag_code = readSpv(frag_path);
    VkShaderModule vert_module = createShaderModule(vert_code);
    VkShaderModule frag_module = createShaderModule(frag_code);

    VkPipelineShaderStageCreateInfo vert_info{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    vert_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_info.module = vert_module;
    vert_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_info{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    frag_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_info.module = frag_module;
    frag_info.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = { vert_info, frag_info };

    auto binding = Vertex::bindingDescription();
    auto attrs = Vertex::attributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vertex_input{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vertex_input.pVertexAttributeDescriptions = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f; viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain_extent_.width);
    viewport.height = static_cast<float>(swapchain_extent_.height);
    viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchain_extent_;

    VkPipelineViewportStateCreateInfo viewport_state{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo raster{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    raster.depthClampEnable = VK_FALSE;
    raster.rasterizerDiscardEnable = VK_FALSE;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_BACK_BIT;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.depthBiasEnable = VK_FALSE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo msaa{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    msaa.minSampleShading = 1.0f;

    VkPipelineColorBlendAttachmentState blend_att{};
    blend_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend_att.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo blend{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    blend.logicOpEnable = VK_FALSE;
    blend.attachmentCount = 1;
    blend.pAttachments = &blend_att;

    VkPushConstantRange push_range{};
    push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    push_range.offset = 0;
    push_range.size = sizeof(float) * 4;

    VkPipelineLayoutCreateInfo layout_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    layout_info.setLayoutCount = 0;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_range;
    if (vkCreatePipelineLayout(device_, &layout_info, nullptr, &pipeline_layout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    VkGraphicsPipelineCreateInfo gp{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    gp.stageCount = 2;
    gp.pStages = stages;
    gp.pVertexInputState = &vertex_input;
    gp.pInputAssemblyState = &input_assembly;
    gp.pViewportState = &viewport_state;
    gp.pRasterizationState = &raster;
    gp.pMultisampleState = &msaa;
    gp.pDepthStencilState = nullptr;
    gp.pColorBlendState = &blend;
    gp.pDynamicState = nullptr;
    gp.layout = pipeline_layout_;
    gp.renderPass = render_pass_;
    gp.subpass = 0;

    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &gp, nullptr, &graphics_pipeline_) != VK_SUCCESS) {
        vkDestroyShaderModule(device_, vert_module, nullptr);
        vkDestroyShaderModule(device_, frag_module, nullptr);
        throw std::runtime_error("Failed to create graphics pipeline");
    }
    setDebugName(device_, VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)pipeline_layout_, "PipelineLayout");
    setDebugName(device_, VK_OBJECT_TYPE_PIPELINE, (uint64_t)graphics_pipeline_, "GraphicsPipeline");

    vkDestroyShaderModule(device_, vert_module, nullptr);
    vkDestroyShaderModule(device_, frag_module, nullptr);
}

void VulkanApp::createFramebuffers() {
    swapchain_framebuffers_.resize(swapchain_image_views_.size());
    for (size_t i = 0; i < swapchain_image_views_.size(); ++i) {
        VkImageView attachments[] = { swapchain_image_views_[i] };
        VkFramebufferCreateInfo info{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        info.renderPass = render_pass_;
        info.attachmentCount = 1;
        info.pAttachments = attachments;
        info.width = swapchain_extent_.width;
        info.height = swapchain_extent_.height;
        info.layers = 1;
        if (vkCreateFramebuffer(device_, &info, nullptr, &swapchain_framebuffers_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
        char name[64]{};
        std::snprintf(name, sizeof(name), "Framebuffer %zu", i);
        setDebugName(device_, VK_OBJECT_TYPE_FRAMEBUFFER, (uint64_t)swapchain_framebuffers_[i], name);
    }
}

void VulkanApp::createCommandPool() {
    VkCommandPoolCreateInfo info{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    info.queueFamilyIndex = queue_family_indices_.graphics.value();
    info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(device_, &info, nullptr, &command_pool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }
    setDebugName(device_, VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)command_pool_, "CommandPool");
}

void VulkanApp::createVertexBuffer() {
    const std::array<Vertex, 3> vertices = {
        Vertex{ glm::vec2{-0.5f, -0.5f} },
        Vertex{ glm::vec2{ 0.5f, -0.5f} },
        Vertex{ glm::vec2{ 0.0f,  0.5f} }
    };

    VkDeviceSize buffer_size = sizeof(vertices);

    VkBufferCreateInfo info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    info.size = buffer_size;
    info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &info, nullptr, &vertex_buffer_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vertex buffer");
    }

    VkMemoryRequirements mem_req{};
    vkGetBufferMemoryRequirements(device_, vertex_buffer_, &mem_req);

    VkMemoryAllocateInfo alloc{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    alloc.allocationSize = mem_req.size;
    alloc.memoryTypeIndex = findMemoryType(physical_device_, mem_req.memoryTypeBits,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(device_, &alloc, nullptr, &vertex_buffer_memory_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate vertex buffer memory");
    }
    vkBindBufferMemory(device_, vertex_buffer_, vertex_buffer_memory_, 0);
    setDebugName(device_, VK_OBJECT_TYPE_BUFFER, (uint64_t)vertex_buffer_, "VertexBuffer");

    void* data = nullptr;
    vkMapMemory(device_, vertex_buffer_memory_, 0, buffer_size, 0, &data);
    std::memcpy(data, vertices.data(), static_cast<size_t>(buffer_size));
    vkUnmapMemory(device_, vertex_buffer_memory_);
}

void VulkanApp::createCommandBuffers() {
    command_buffers_.resize(swapchain_framebuffers_.size());
    VkCommandBufferAllocateInfo alloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    alloc.commandPool = command_pool_;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = static_cast<uint32_t>(command_buffers_.size());
    if (vkAllocateCommandBuffers(device_, &alloc, command_buffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }
    for (size_t i = 0; i < command_buffers_.size(); ++i) {
        char name[64]{};
        std::snprintf(name, sizeof(name), "Cmd %zu", i);
        setDebugName(device_, VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)command_buffers_[i], name);
    }
}

void VulkanApp::createSyncObjects() {
    image_available_semaphores_.resize(kMaxFramesInFlight);
    render_finished_semaphores_.resize(kMaxFramesInFlight);
    in_flight_fences_.resize(kMaxFramesInFlight);
    images_in_flight_.assign(swapchain_images_.size(), VK_NULL_HANDLE);
    render_finished_image_semaphores_.resize(swapchain_images_.size());

    VkSemaphoreCreateInfo sem{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fence{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (vkCreateSemaphore(device_, &sem, nullptr, &image_available_semaphores_[i]) != VK_SUCCESS ||
            vkCreateFence(device_, &fence, nullptr, &in_flight_fences_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create sync objects");
        }
    }
    for (size_t i = 0; i < render_finished_image_semaphores_.size(); ++i) {
        if (vkCreateSemaphore(device_, &sem, nullptr, &render_finished_image_semaphores_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create per-image render-finished semaphores");
        }
    }
}

void VulkanApp::createDescriptorPool() {
    VkDescriptorPoolCreateInfo info{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    info.maxSets = 1000 * static_cast<uint32_t>(kImGuiPoolSizes.size());
    info.poolSizeCount = static_cast<uint32_t>(kImGuiPoolSizes.size());
    info.pPoolSizes = kImGuiPoolSizes.data();
    if (vkCreateDescriptorPool(device_, &info, nullptr, &imgui_descriptor_pool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui descriptor pool");
    }
    setDebugName(device_, VK_OBJECT_TYPE_DESCRIPTOR_POOL, (uint64_t)imgui_descriptor_pool_, "ImGuiDescriptorPool");
}

void VulkanApp::recreateSwapchain() {
    int width = 0, height = 0;
    do {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEventsTimeout(0.01);
    } while (width == 0 || height == 0);

    vkDeviceWaitIdle(device_);
    cleanupSwapchain();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createPipeline();
    createFramebuffers();
    createCommandBuffers();
    images_in_flight_.assign(swapchain_images_.size(), VK_NULL_HANDLE);
    // Recreate per-image semaphores
    for (auto s : render_finished_image_semaphores_) { vkDestroySemaphore(device_, s, nullptr); }
    render_finished_image_semaphores_.resize(swapchain_images_.size());
    VkSemaphoreCreateInfo sem{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    for (size_t i = 0; i < render_finished_image_semaphores_.size(); ++i) {
        if (vkCreateSemaphore(device_, &sem, nullptr, &render_finished_image_semaphores_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to recreate per-image semaphores");
        }
    }
}

void VulkanApp::cleanupSwapchain() {
    for (auto fb : swapchain_framebuffers_) { vkDestroyFramebuffer(device_, fb, nullptr); }
    swapchain_framebuffers_.clear();
    vkDestroyPipeline(device_, graphics_pipeline_, nullptr); graphics_pipeline_ = VK_NULL_HANDLE;
    vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr); pipeline_layout_ = VK_NULL_HANDLE;
    vkDestroyRenderPass(device_, render_pass_, nullptr); render_pass_ = VK_NULL_HANDLE;
    for (auto iv : swapchain_image_views_) { vkDestroyImageView(device_, iv, nullptr); }
    swapchain_image_views_.clear();
    vkDestroySwapchainKHR(device_, swapchain_, nullptr); swapchain_ = VK_NULL_HANDLE;
    if (!command_buffers_.empty()) { vkFreeCommandBuffers(device_, command_pool_, static_cast<uint32_t>(command_buffers_.size()), command_buffers_.data()); }
    command_buffers_.clear();
}

void VulkanApp::drawFrame(const glm::vec4& color) {
    vkWaitForFences(device_, 1, &in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX);
    vkResetFences(device_, 1, &in_flight_fences_[current_frame_]);

    VkResult acq = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, image_available_semaphores_[current_frame_], VK_NULL_HANDLE, &current_image_index_);
    if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }
    if (images_in_flight_[current_image_index_] != VK_NULL_HANDLE) {
        vkWaitForFences(device_, 1, &images_in_flight_[current_image_index_], VK_TRUE, UINT64_MAX);
    }
    images_in_flight_[current_image_index_] = in_flight_fences_[current_frame_];

    VkCommandBuffer cmd = command_buffers_[current_image_index_];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo begin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS) { throw std::runtime_error("Begin cmd buffer failed"); }

    VkClearValue clear{};
    clear.color = { {0.0f, 0.0f, 0.0f, 1.0f} };

    VkRenderPassBeginInfo rp{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    rp.renderPass = render_pass_;
    rp.framebuffer = swapchain_framebuffers_[current_image_index_];
    rp.renderArea.offset = {0, 0};
    rp.renderArea.extent = swapchain_extent_;
    rp.clearValueCount = 1;
    rp.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    const float passCol[4] = {0.1f, 0.2f, 0.8f, 1.0f};
    cmdBeginLabel(device_, cmd, "MainPass", passCol);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_);
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer_, offsets);
    vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float) * 4, &color[0]);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    // Render ImGui on top
    ImGui::GetDrawData(); // ensure frame started by caller
    // The caller will have called ImGui::NewFrame/Render. Just render draw data here
    const float uiCol[4] = {0.8f, 0.7f, 0.2f, 1.0f};
    cmdBeginLabel(device_, cmd, "ImGui", uiCol);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    cmdEndLabel(device_, cmd);

    cmdEndLabel(device_, cmd);
    vkCmdEndRenderPass(cmd);
    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) { throw std::runtime_error("End cmd buffer failed"); }

    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &image_available_semaphores_[current_frame_];
    submit.pWaitDstStageMask = wait_stages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &render_finished_image_semaphores_[current_image_index_];

    if (vkQueueSubmit(graphics_queue_, 1, &submit, in_flight_fences_[current_frame_]) != VK_SUCCESS) {
        throw std::runtime_error("Queue submit failed");
    }

    VkPresentInfoKHR present{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &render_finished_image_semaphores_[current_image_index_];
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain_;
    present.pImageIndices = &current_image_index_;

    VkResult pres = vkQueuePresentKHR(present_queue_, &present);
    if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
    } else if (pres != VK_SUCCESS) {
        throw std::runtime_error("Present failed");
    }

    current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;
}
