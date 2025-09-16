#include <vulkano/app.hpp>

#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <volk.h>
#include <vk_mem_alloc.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <glm/glm.hpp>
#ifdef VULKANO_CODEX_USE_SHADERC
#include <shaderc/shaderc.hpp>
#endif

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>

// Forward declare debug name helper
void setDebugName(VkDevice device, VkObjectType type, uint64_t handle, const char* name);

namespace vulkano {

namespace {

constexpr int defaultWidth {1280};
constexpr int defaultHeight {720};

void glfwErrorCallback(const int code, const char* description) noexcept {
    (void)code;
    (void)description;
}

[[nodiscard]] std::vector<const char*> getGlfwRequiredInstanceExtensions() {
    uint32_t count {0U};
    const char** names = glfwGetRequiredInstanceExtensions(&count);
    std::vector<const char*> result {};
    result.reserve(static_cast<std::size_t>(count));
    for (uint32_t i = 0; i < count; ++i) {
        result.push_back(names[i]);
    }
    return result;
}

} // namespace

struct App::Impl {
    AppConfig config {};
    bool running {false};

    GLFWwindow* window {nullptr};

    VkInstance instance {VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT debugMessenger {VK_NULL_HANDLE};
    VkSurfaceKHR surface {VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice {VK_NULL_HANDLE};
    VkDevice device {VK_NULL_HANDLE};
    uint32_t queueFamilyGraphics {0U};
    uint32_t queueFamilyPresent {0U};
    VkQueue graphicsQueue {VK_NULL_HANDLE};
    VkQueue presentQueue {VK_NULL_HANDLE};

    VmaAllocator allocator {VK_NULL_HANDLE};

    VkSwapchainKHR swapchain {VK_NULL_HANDLE};
    VkFormat swapchainFormat {VK_FORMAT_B8G8R8A8_UNORM};
    VkExtent2D swapchainExtent {0U, 0U};
    std::vector<VkImage> swapchainImages {};
    std::vector<VkImageView> swapchainImageViews {};

    VkRenderPass renderPass {VK_NULL_HANDLE};
    std::vector<VkFramebuffer> framebuffers {};

    VkCommandPool commandPool {VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> commandBuffers {};

    VkSemaphore imageAvailableSemaphore {VK_NULL_HANDLE};
    VkSemaphore renderFinishedSemaphore {VK_NULL_HANDLE};
    VkFence inFlightFence {VK_NULL_HANDLE};

    // Pipeline and buffers
    VkPipelineLayout pipelineLayout {VK_NULL_HANDLE};
    VkPipeline pipeline {VK_NULL_HANDLE};
    VkBuffer vertexBuffer {VK_NULL_HANDLE};
    VmaAllocation vertexAllocation {VK_NULL_HANDLE};

    // ImGui
    VkDescriptorPool imguiDescriptorPool {VK_NULL_HANDLE};
    bool imguiInitialized {false};

    // Debug utils labels
    PFN_vkCmdBeginDebugUtilsLabelEXT pfnCmdBeginLabel {nullptr};
    PFN_vkCmdEndDebugUtilsLabelEXT pfnCmdEndLabel {nullptr};

    // Timing for FPS/frame time
    std::chrono::steady_clock::time_point lastFrameTime {std::chrono::steady_clock::now()};
    std::vector<double> dtSamples {};
    int dtWindow {100};

    explicit Impl(const AppConfig& cfg)
        : config {cfg} {
    }

    void initGlfw() {
        glfwSetErrorCallback(&glfwErrorCallback);
        if (!glfwInit()) {
            throw std::runtime_error {"Failed to initialize GLFW"};
        }
        if (glfwVulkanSupported() != GLFW_TRUE) {
            throw std::runtime_error {"GLFW reports Vulkan not supported"};
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        const int width = (config.initialWidth > 0) ? config.initialWidth : defaultWidth;
        const int height = (config.initialHeight > 0) ? config.initialHeight : defaultHeight;
        window = glfwCreateWindow(width, height, config.windowTitle.c_str(), nullptr, nullptr);
        if (window == nullptr) {
            throw std::runtime_error {"Failed to create GLFW window"};
        }
    }

    void initVulkan() {
        if (volkInitialize() != VK_SUCCESS) {
            throw std::runtime_error {"Failed to initialize volk"};
        }

        vkb::InstanceBuilder builder {};
        builder.require_api_version(1, 2, 0);
        builder.set_app_name(config.windowTitle.c_str());

        const bool enableValidation = config.enableValidation;
#ifdef VULKANO_CODEX_DEBUG
        if (enableValidation) {
            builder.request_validation_layers(true).use_default_debug_messenger();
        }
#endif

        const auto glfwExts = getGlfwRequiredInstanceExtensions();
        for (const char* ext : glfwExts) {
            builder.enable_extension(ext);
        }

        auto instRet = builder.build();
        if (!instRet) {
            throw std::runtime_error {"Failed to create Vulkan instance"};
        }
        vkb::Instance vkbInstance = instRet.value();
        instance = vkbInstance.instance;
        debugMessenger = vkbInstance.debug_messenger;
        volkLoadInstance(instance);

        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create window surface"};
        }

        vkb::PhysicalDeviceSelector selector {vkbInstance};
        auto physRet = selector.set_surface(surface)
                                 .set_minimum_version(1, 2)
                                 .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
                                 .select();
        if (!physRet) {
            throw std::runtime_error {"Failed to select physical device"};
        }
        vkb::PhysicalDevice vkbPhys = physRet.value();
        physicalDevice = vkbPhys.physical_device;

        vkb::DeviceBuilder devBuilder {vkbPhys};
        auto devRet = devBuilder.build();
        if (!devRet) {
            throw std::runtime_error {"Failed to create logical device"};
        }
        vkb::Device vkbDevice = devRet.value();
        device = vkbDevice.device;
        graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
        presentQueue = vkbDevice.get_queue(vkb::QueueType::present).value();
        queueFamilyGraphics = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
        queueFamilyPresent = vkbDevice.get_queue_index(vkb::QueueType::present).value();

        setDebugName(device, VK_OBJECT_TYPE_DEVICE, reinterpret_cast<uint64_t>(device), "LogicalDevice");
        setDebugName(device, VK_OBJECT_TYPE_QUEUE, reinterpret_cast<uint64_t>(graphicsQueue), "GraphicsQueue");
        setDebugName(device, VK_OBJECT_TYPE_QUEUE, reinterpret_cast<uint64_t>(presentQueue), "PresentQueue");

        // Resolve debug label functions (optional)
        pfnCmdBeginLabel = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT"));
        pfnCmdEndLabel = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT"));
    }

    void init() {
        initGlfw();
        initVulkan();
        initAllocator();
        createSwapchain();
        createRenderPass();
        createFramebuffers();
        createCommandPoolAndBuffers();
        createSyncObjects();
        createTriangleResources();
        initImGui();
        running = true;
    }

    void mainLoop() {
        while (running && glfwWindowShouldClose(window) == GLFW_FALSE) {
            glfwPollEvents();
            drawFrame();
        }
        if (device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
        }
    }

    void shutdown() noexcept {
        if (device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
        }
        // Destroy ImGui
        if (imguiInitialized) {
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            imguiInitialized = false;
        }
        if (imguiDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, imguiDescriptorPool, nullptr);
            imguiDescriptorPool = VK_NULL_HANDLE;
        }
        destroyPipeline();
        if (inFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(device, inFlightFence, nullptr);
            inFlightFence = VK_NULL_HANDLE;
        }
        if (imageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
            imageAvailableSemaphore = VK_NULL_HANDLE;
        }
        if (renderFinishedSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
            renderFinishedSemaphore = VK_NULL_HANDLE;
        }
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }
        if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
        }
        destroySwapchain();
        if (allocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(allocator);
            allocator = VK_NULL_HANDLE;
        }
        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
            surface = VK_NULL_HANDLE;
        }
        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }
        if (instance != VK_NULL_HANDLE) {
            if (debugMessenger != VK_NULL_HANDLE) {
                auto pfnDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
                if (pfnDestroyDebugUtilsMessengerEXT != nullptr) {
                    pfnDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
                }
            }
            vkDestroyInstance(instance, nullptr);
            instance = VK_NULL_HANDLE;
        }
        if (window != nullptr) {
            glfwDestroyWindow(window);
            window = nullptr;
        }
        glfwTerminate();
        running = false;
    }

    void initAllocator() {
        VmaVulkanFunctions funcs {};
        funcs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        funcs.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
        VmaAllocatorCreateInfo info {};
        info.instance = instance;
        info.physicalDevice = physicalDevice;
        info.device = device;
        info.pVulkanFunctions = &funcs;
        info.vulkanApiVersion = VK_API_VERSION_1_2;
        if (vmaCreateAllocator(&info, &allocator) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create VMA allocator"};
        }
    }

    void createSwapchain() {
        int width {0};
        int height {0};
        glfwGetFramebufferSize(window, &width, &height);
        vkb::SwapchainBuilder builder {physicalDevice, device, surface};
        auto scRet = builder.use_default_format_selection()
                           .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                           .set_desired_extent(static_cast<uint32_t>(width), static_cast<uint32_t>(height))
                           .build();
        if (!scRet) {
            throw std::runtime_error {"Failed to create swapchain"};
        }
        vkb::Swapchain vkbSwap = scRet.value();
        swapchain = vkbSwap.swapchain;
        swapchainImages = vkbSwap.get_images().value();
        swapchainImageViews = vkbSwap.get_image_views().value();
        swapchainFormat = vkbSwap.image_format;
        swapchainExtent = vkbSwap.extent;
        setDebugName(device, VK_OBJECT_TYPE_SWAPCHAIN_KHR, reinterpret_cast<uint64_t>(swapchain), "Swapchain");
        for (std::size_t i = 0; i < swapchainImageViews.size(); ++i) {
            setDebugName(device, VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64_t>(swapchainImageViews[i]), "SwapchainImageView");
        }
    }

    void destroySwapchain() noexcept {
        for (VkFramebuffer fb : framebuffers) {
            vkDestroyFramebuffer(device, fb, nullptr);
        }
        framebuffers.clear();
        for (VkImageView iv : swapchainImageViews) {
            vkDestroyImageView(device, iv, nullptr);
        }
        swapchainImageViews.clear();
        if (swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, swapchain, nullptr);
            swapchain = VK_NULL_HANDLE;
        }
    }

    void recreateSwapchain() {
        vkDeviceWaitIdle(device);
        destroySwapchain();
        createSwapchain();
        createRenderPass();
        createFramebuffers();
        allocateCommandBuffers();
        // Recreate dependent resources
        destroyPipeline();
        createTriangleResources();
    }

    void createRenderPass() {
        if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
        }
        VkAttachmentDescription colorAttachment {};
        colorAttachment.format = swapchainFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

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
        dep.srcAccessMask = 0U;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo rpInfo {};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.attachmentCount = 1U;
        rpInfo.pAttachments = &colorAttachment;
        rpInfo.subpassCount = 1U;
        rpInfo.pSubpasses = &subpass;
        rpInfo.dependencyCount = 1U;
        rpInfo.pDependencies = &dep;
        if (vkCreateRenderPass(device, &rpInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create render pass"};
        }
        setDebugName(device, VK_OBJECT_TYPE_RENDER_PASS, reinterpret_cast<uint64_t>(renderPass), "RenderPass");
    }

    void createFramebuffers() {
        framebuffers.clear();
        framebuffers.reserve(swapchainImageViews.size());
        for (VkImageView view : swapchainImageViews) {
            VkImageView attachments[] {view};
            VkFramebufferCreateInfo fbInfo {};
            fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbInfo.renderPass = renderPass;
            fbInfo.attachmentCount = 1U;
            fbInfo.pAttachments = attachments;
            fbInfo.width = swapchainExtent.width;
            fbInfo.height = swapchainExtent.height;
            fbInfo.layers = 1U;
            VkFramebuffer fb {VK_NULL_HANDLE};
            if (vkCreateFramebuffer(device, &fbInfo, nullptr, &fb) != VK_SUCCESS) {
                throw std::runtime_error {"Failed to create framebuffer"};
            }
            framebuffers.push_back(fb);
        }
    }

    void createCommandPoolAndBuffers() {
        if (commandPool == VK_NULL_HANDLE) {
            VkCommandPoolCreateInfo poolInfo {};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.queueFamilyIndex = queueFamilyGraphics;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create command pool"};
        }
        setDebugName(device, VK_OBJECT_TYPE_COMMAND_POOL, reinterpret_cast<uint64_t>(commandPool), "CommandPool");
        }
        allocateCommandBuffers();
    }

    void allocateCommandBuffers() {
        commandBuffers.clear();
        commandBuffers.resize(swapchainImages.size());
        VkCommandBufferAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to allocate command buffers"};
        }
    }

    void createSyncObjects() {
        VkSemaphoreCreateInfo semInfo {};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if (vkCreateSemaphore(device, &semInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create semaphore"};
        }
        if (vkCreateSemaphore(device, &semInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create semaphore"};
        }
        VkFenceCreateInfo fenceInfo {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        if (vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create fence"};
        }
    }

    void recordCommandBuffer(const VkCommandBuffer cb, const uint32_t imageIndex) {
        (void)imageIndex;
        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(cb, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to begin command buffer"};
        }

        beginLabel(cb, "Frame");

        // Build ImGui frame
        if (imguiInitialized) {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            drawStatsUi();
            ImGui::Render();
        }

        VkClearValue clear {}; // black with full alpha
        clear.color = {{0.0F, 0.0F, 0.0F, 1.0F}};
        VkRenderPassBeginInfo rpBegin {};
        rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBegin.renderPass = renderPass;
        rpBegin.framebuffer = framebuffers[imageIndex];
        rpBegin.renderArea.offset = {0, 0};
        rpBegin.renderArea.extent = swapchainExtent;
        rpBegin.clearValueCount = 1U;
        rpBegin.pClearValues = &clear;
        vkCmdBeginRenderPass(cb, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

        if (pipeline != VK_NULL_HANDLE && vertexBuffer != VK_NULL_HANDLE) {
            beginLabel(cb, "Triangle");
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            VkDeviceSize offsets[] {0U};
            vkCmdBindVertexBuffers(cb, 0U, 1U, &vertexBuffer, offsets);
            const float white[4] {1.0F, 1.0F, 1.0F, 1.0F};
            vkCmdPushConstants(cb, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0U, sizeof(white), white);
            vkCmdDraw(cb, 3U, 1U, 0U, 0U);
            endLabel(cb);
        }

        if (imguiInitialized) {
            beginLabel(cb, "ImGui");
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cb);
            endLabel(cb);
        }

        vkCmdEndRenderPass(cb);

        endLabel(cb);

        if (vkEndCommandBuffer(cb) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to record command buffer"};
        }
    }

    void drawFrame() {
        vkWaitForFences(device, 1U, &inFlightFence, VK_TRUE, UINT64_C(1'000'000'000));
        vkResetFences(device, 1U, &inFlightFence);

        uint32_t imageIndex {0U};
        const VkResult acquireRes = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
        if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            return;
        }
        if (acquireRes != VK_SUCCESS && acquireRes != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error {"Failed to acquire swapchain image"};
        }

        vkResetCommandBuffer(commandBuffers[imageIndex], 0U);
        recordCommandBuffer(commandBuffers[imageIndex], imageIndex);

        VkPipelineStageFlags waitStages[] {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1U;
        submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1U;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
        submitInfo.signalSemaphoreCount = 1U;
        submitInfo.pSignalSemaphores = &renderFinishedSemaphore;

        if (vkQueueSubmit(graphicsQueue, 1U, &submitInfo, inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to submit draw"};
        }

        VkPresentInfoKHR presentInfo {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1U;
        presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
        presentInfo.swapchainCount = 1U;
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &imageIndex;
        const VkResult pres = vkQueuePresentKHR(presentQueue, &presentInfo);
        if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) {
            recreateSwapchain();
        } else if (pres != VK_SUCCESS) {
            throw std::runtime_error {"Failed to present"};
        }
    }

    void drawStatsUi() {
        // Update timing
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<double> dt = now - lastFrameTime;
        lastFrameTime = now;
        const double seconds = dt.count();
        if (static_cast<int>(dtSamples.size()) == dtWindow) {
            dtSamples.erase(dtSamples.begin());
        }
        dtSamples.push_back(seconds);
        double sum {0.0};
        for (double v : dtSamples) {
            sum += v;
        }
        const double avg = (dtSamples.empty() ? 0.0 : sum / static_cast<double>(dtSamples.size()));
        const double ms = avg * 1000.0;
        const double fps = (ms > 0.0 ? 1000.0 / ms : 0.0);

        VkPhysicalDeviceProperties props {};
        vkGetPhysicalDeviceProperties(physicalDevice, &props);

        ImGui::Begin("Stats");
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Frame time: %.3f ms", ms);
        ImGui::Text("Device: %s", props.deviceName);
        ImGui::Text("Extent: %u x %u", swapchainExtent.width, swapchainExtent.height);
        ImGui::End();
    }

    void beginLabel(const VkCommandBuffer cb, const char* name) const noexcept {
#ifdef VULKANO_CODEX_DEBUG
        if (pfnCmdBeginLabel != nullptr) {
            VkDebugUtilsLabelEXT label {};
            label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
            label.pLabelName = name;
            pfnCmdBeginLabel(cb, &label);
            return;
        }
#endif
        (void)cb;
        (void)name;
    }

    void endLabel(const VkCommandBuffer cb) const noexcept {
#ifdef VULKANO_CODEX_DEBUG
        if (pfnCmdEndLabel != nullptr) {
            pfnCmdEndLabel(cb);
            return;
        }
#endif
        (void)cb;
    }

    static std::vector<uint32_t> compileGlslToSpv(const std::string& src, const VkShaderStageFlagBits stage) {
#ifdef VULKANO_CODEX_USE_SHADERC
        shaderc_shader_kind kind = shaderc_glsl_vertex_shader;
        if (stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
            kind = shaderc_glsl_fragment_shader;
        }
        shaderc::Compiler compiler {};
        shaderc::CompileOptions options {};
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
        const shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(src, kind, "shader.glsl", options);
        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            throw std::runtime_error {std::string {"Shader compilation failed: "} + result.GetErrorMessage()};
        }
        return {result.cbegin(), result.cend()};
#else
        (void)src;
        (void)stage;
        throw std::runtime_error {"Shader compilation disabled. Enable VULKANO_CODEX_USE_SHADERC to compile GLSL."};
#endif
    }

    static std::vector<uint32_t> loadSpirvFromFile(const std::string& path) {
        std::vector<uint32_t> code {};
        FILE* f = std::fopen(path.c_str(), "rb");
        if (f == nullptr) {
            throw std::runtime_error {"Failed to open SPIR-V file: " + path};
        }
        std::fseek(f, 0, SEEK_END);
        const long size = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        if (size <= 0 || (size % 4) != 0) {
            std::fclose(f);
            throw std::runtime_error {"Invalid SPIR-V file size: " + path};
        }
        code.resize(static_cast<std::size_t>(size) / 4U);
        const std::size_t read = std::fread(code.data(), 1, static_cast<std::size_t>(size), f);
        std::fclose(f);
        if (read != static_cast<std::size_t>(size)) {
            throw std::runtime_error {"Failed to read SPIR-V file: " + path};
        }
        return code;
    }

    static VkShaderModule createShaderModule(const VkDevice device, const std::vector<uint32_t>& code) {
        VkShaderModuleCreateInfo info {};
        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = code.size() * sizeof(uint32_t);
        info.pCode = code.data();
        VkShaderModule module {VK_NULL_HANDLE};
        if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create shader module"};
        }
        return module;
    }

    void createTriangleResources() {
        // Shaders (vertex + fragment with push constant color)
        const std::string vertSrc {
            "#version 450\n"
            "layout(location=0) in vec2 inPos;\n"
            "void main(){ gl_Position = vec4(inPos, 0.0, 1.0); }\n"};
        const std::string fragSrc {
            "#version 450\n"
            "layout(push_constant) uniform Push { vec4 color; } pc;\n"
            "layout(location=0) out vec4 outColor;\n"
            "void main(){ outColor = pc.color; }\n"};

        VkShaderModule vertModule {VK_NULL_HANDLE};
        VkShaderModule fragModule {VK_NULL_HANDLE};
        try {
            const auto vertSpv = compileGlslToSpv(vertSrc, VK_SHADER_STAGE_VERTEX_BIT);
            const auto fragSpv = compileGlslToSpv(fragSrc, VK_SHADER_STAGE_FRAGMENT_BIT);
            vertModule = createShaderModule(device, vertSpv);
            fragModule = createShaderModule(device, fragSpv);
        } catch (const std::exception&) {
            // Fallback: load precompiled SPIR-V from disk (bin/shaders)
            const std::string baseDir = std::string {VULKANO_SHADER_DIR};
            const std::string vpath = baseDir + "/triangle.vert.spv";
            const std::string fpath = baseDir + "/triangle.frag.spv";
            const auto vertSpv = loadSpirvFromFile(vpath);
            const auto fragSpv = loadSpirvFromFile(fpath);
            vertModule = createShaderModule(device, vertSpv);
            fragModule = createShaderModule(device, fragSpv);
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

        const VkPipelineShaderStageCreateInfo stages[] {vertStage, fragStage};

        // Vertex input: vec2 positions
        VkVertexInputBindingDescription binding {};
        binding.binding = 0U;
        binding.stride = sizeof(glm::vec2);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attrib {};
        attrib.location = 0U;
        attrib.binding = 0U;
        attrib.format = VK_FORMAT_R32G32_SFLOAT;
        attrib.offset = 0U;

        VkPipelineVertexInputStateCreateInfo vertexInput {};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = 1U;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = 1U;
        vertexInput.pVertexAttributeDescriptions = &attrib;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport {};
        viewport.x = 0.0F;
        viewport.y = 0.0F;
        viewport.width = static_cast<float>(swapchainExtent.width);
        viewport.height = static_cast<float>(swapchainExtent.height);
        viewport.minDepth = 0.0F;
        viewport.maxDepth = 1.0F;

        VkRect2D scissor {};
        scissor.offset = {0, 0};
        scissor.extent = swapchainExtent;

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
        raster.cullMode = VK_CULL_MODE_BACK_BIT;
        raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
        raster.depthBiasEnable = VK_FALSE;
        raster.lineWidth = 1.0F;

        VkPipelineMultisampleStateCreateInfo msaa {};
        msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState blendAttachment {};
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo blend {};
        blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1U;
        blend.pAttachments = &blendAttachment;

        VkPushConstantRange pushRange {};
        pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.offset = 0U;
        pushRange.size = sizeof(float) * 4U;

        VkPipelineLayoutCreateInfo layoutInfo {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.pushConstantRangeCount = 1U;
        layoutInfo.pPushConstantRanges = &pushRange;
        if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            vkDestroyShaderModule(device, vertModule, nullptr);
            vkDestroyShaderModule(device, fragModule, nullptr);
            throw std::runtime_error {"Failed to create pipeline layout"};
        }

        VkGraphicsPipelineCreateInfo pipeInfo {};
        pipeInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeInfo.stageCount = 2U;
        pipeInfo.pStages = stages;
        pipeInfo.pVertexInputState = &vertexInput;
        pipeInfo.pInputAssemblyState = &inputAssembly;
        pipeInfo.pViewportState = &viewportState;
        pipeInfo.pRasterizationState = &raster;
        pipeInfo.pMultisampleState = &msaa;
        pipeInfo.pColorBlendState = &blend;
        pipeInfo.layout = pipelineLayout;
        pipeInfo.renderPass = renderPass;
        pipeInfo.subpass = 0U;
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1U, &pipeInfo, nullptr, &pipeline) != VK_SUCCESS) {
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
            vkDestroyShaderModule(device, vertModule, nullptr);
            vkDestroyShaderModule(device, fragModule, nullptr);
            throw std::runtime_error {"Failed to create graphics pipeline"};
        }

        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);

        createVertexBuffer();
    }

    void destroyPipeline() noexcept {
        if (vertexBuffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator, vertexBuffer, vertexAllocation);
            vertexBuffer = VK_NULL_HANDLE;
            vertexAllocation = VK_NULL_HANDLE;
        }
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }
        if (pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }
    }

    void createVertexBuffer() {
        const std::array<glm::vec2, 3> vertices {
            glm::vec2 {0.0F, -0.5F},
            glm::vec2 {0.5F, 0.5F},
            glm::vec2 {-0.5F, 0.5F},
        };

        VkBufferCreateInfo bufInfo {};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = sizeof(vertices);
        bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo allocInfo {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        VmaAllocationInfo vinfo {};
        if (vmaCreateBuffer(allocator, &bufInfo, &allocInfo, &vertexBuffer, &vertexAllocation, &vinfo) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to allocate vertex buffer"};
        }
        std::memcpy(vinfo.pMappedData, vertices.data(), sizeof(vertices));
    }

    void initImGui() {
        // Descriptor pool for ImGui
        VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};
        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 1000 * static_cast<uint32_t>(std::size(poolSizes));
        poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
        poolInfo.pPoolSizes = poolSizes;
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &imguiDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create ImGui descriptor pool"};
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForVulkan(window, true);

        ImGui_ImplVulkan_InitInfo initInfo {};
        initInfo.Instance = instance;
        initInfo.PhysicalDevice = physicalDevice;
        initInfo.Device = device;
        initInfo.QueueFamily = queueFamilyGraphics;
        initInfo.Queue = graphicsQueue;
        initInfo.PipelineCache = VK_NULL_HANDLE;
        initInfo.DescriptorPool = imguiDescriptorPool;
        initInfo.Allocator = nullptr;
        initInfo.MinImageCount = static_cast<uint32_t>(swapchainImages.size());
        initInfo.ImageCount = static_cast<uint32_t>(swapchainImages.size());
        initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        initInfo.CheckVkResultFn = nullptr;
        initInfo.RenderPass = renderPass;
        ImGui_ImplVulkan_Init(&initInfo);

        VkCommandBufferAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1U;
        VkCommandBuffer fontCmd {};
        vkAllocateCommandBuffers(device, &allocInfo, &fontCmd);

        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(fontCmd, &beginInfo);
        ImGui_ImplVulkan_CreateFontsTexture();
        vkEndCommandBuffer(fontCmd);

        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1U;
        submitInfo.pCommandBuffers = &fontCmd;
        vkQueueSubmit(graphicsQueue, 1U, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);
        ImGui_ImplVulkan_DestroyFontsTexture();

        imguiInitialized = true;
    }
};

App::App(const AppConfig& config)
    : impl {std::make_unique<Impl>(config)} {
    impl->init();
}

App::~App() {
    impl->shutdown();
}

void App::run() {
    impl->mainLoop();
}

} // namespace vulkano
void setDebugName(const VkDevice device, const VkObjectType type, const uint64_t handle, const char* name) {
#ifdef VULKANO_CODEX_DEBUG
    const auto pfn = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
        vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
    if (pfn != nullptr) {
        VkDebugUtilsObjectNameInfoEXT info {};
        info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        info.objectType = type;
        info.objectHandle = handle;
        info.pObjectName = name;
        (void)pfn(device, &info);
    }
#else
    (void)device;
    (void)type;
    (void)handle;
    (void)name;
#endif
}
