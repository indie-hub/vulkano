#include <vulkano/app.hpp>

#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <volk.h>
#include <vk_mem_alloc.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

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

    void recordCommandBuffer(const VkCommandBuffer cb, const uint32_t imageIndex) const {
        (void)imageIndex;
        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(cb, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to begin command buffer"};
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

        // Placeholder: no draw yet (will add pipeline/triangle)

        vkCmdEndRenderPass(cb);

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
