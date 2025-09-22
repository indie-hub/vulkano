#include <vulkano/application.hpp>

#include <array>
#include <limits>
#include <span>
#include <stdexcept>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

namespace {
    constexpr std::uint32_t maxFramesInFlight {2U};
    constexpr std::array<float, 4U> clearColour {0.0F, 0.0F, 0.0F, 1.0F};
    constexpr float timingSmoothingFactor {0.1F};
}

namespace vulkano {

VulkanApplication::VulkanApplication(const AppConfig& config)
    : m_config {config}
    , m_glfwContext {}
    , m_window {Window::create(m_glfwContext, m_config)}
    , m_context {VulkanContextBuilder {}.with_config(m_config).with_window(m_window).build()}
    , m_swapchain {Swapchain::create(m_context, m_window)}
    , m_renderPass {RenderPass::create(m_context, m_swapchain.image_format())}
    , m_framebuffers {FramebufferCollection::create(m_context, m_swapchain, m_renderPass)}
    , m_commandAllocator {CommandAllocator::create(m_context, static_cast<std::uint32_t>(m_framebuffers.size()))}
    , m_syncManager {SyncManager::create(m_context, maxFramesInFlight)}
    , m_deviceName {m_context.device_properties().deviceName} {
    const auto vertices = default_triangle_vertices();
    m_vertexBuffer = Buffer::create_vertex_buffer(m_context, std::span<const Vertex>(vertices.data(), vertices.size()));
    m_pipeline = GraphicsPipeline::create(m_context, m_renderPass);
    m_imagesInFlight.resize(m_swapchain.image_views().size(), VK_NULL_HANDLE);

    init_imgui();
    register_callbacks();
    m_lastFrameTime = std::chrono::steady_clock::now();
}

VulkanApplication::~VulkanApplication() {
    wait_for_device_idle();
    destroy_imgui();
}

void VulkanApplication::run() {
    while(!m_window.should_close()) {
        glfwPollEvents();
        draw_frame();
    }
    wait_for_device_idle();
}

void VulkanApplication::request_close() {
    glfwSetWindowShouldClose(m_window.handle(), GLFW_TRUE);
}

void VulkanApplication::register_callbacks() {
    GLFWwindow* windowHandle = m_window.handle();
    glfwSetWindowUserPointer(windowHandle, this);
    glfwSetFramebufferSizeCallback(windowHandle, &VulkanApplication::framebuffer_resize_callback);
}

void VulkanApplication::framebuffer_resize_callback(GLFWwindow* window, int, int) {
    auto* app = static_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));
    if(app == nullptr) {
        return;
    }
    app->m_framebufferResized = true;
}

void VulkanApplication::draw_frame() {
    const auto now = std::chrono::steady_clock::now();
    const double deltaSeconds = std::chrono::duration<double>(now - m_lastFrameTime).count();
    m_lastFrameTime = now;
    update_timing(deltaSeconds);

    auto& frames = m_syncManager.frames();
    FrameSync& frameSync = frames.at(m_currentFrame);

    if(vkWaitForFences(m_context.device(), 1U, &frameSync.inFlight, VK_TRUE, std::numeric_limits<std::uint64_t>::max()) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to wait for in-flight fence"};
    }

    std::uint32_t imageIndex {0U};
    const VkResult acquireResult = vkAcquireNextImageKHR(
        m_context.device(),
        m_swapchain.handle(),
        std::numeric_limits<std::uint64_t>::max(),
        frameSync.imageAvailable,
        VK_NULL_HANDLE,
        &imageIndex);

    if(acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return;
    }
    if(acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error {"Failed to acquire swapchain image"};
    }

    if(m_imagesInFlight.at(imageIndex) != VK_NULL_HANDLE) {
        if(vkWaitForFences(m_context.device(), 1U, &m_imagesInFlight.at(imageIndex), VK_TRUE, std::numeric_limits<std::uint64_t>::max()) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to wait for fence of previous frame"};
        }
    }
    m_imagesInFlight[imageIndex] = frameSync.inFlight;

    if(vkResetFences(m_context.device(), 1U, &frameSync.inFlight) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to reset in-flight fence"};
    }

    begin_imgui_frame();

    const VkExtent2D extent = m_swapchain.extent();
    ImGui::SetNextWindowBgAlpha(0.35F);
    ImGui::SetNextWindowSize(ImVec2 {0.0F, 0.0F}, ImGuiCond_Always);
    ImGui::Begin("Runtime Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::Text("FPS: %.2f", m_fps);
    ImGui::Text("Frame Time: %.2f ms", m_frameTimeMs);
    ImGui::Text("Device: %s", m_deviceName.c_str());
    ImGui::Text("Swapchain: %u x %u", extent.width, extent.height);
    ImGui::End();

    ImGui::Render();
    record_command_buffer(imageIndex);

    const VkCommandBuffer commandBuffer = m_commandAllocator.buffers().at(imageIndex);
    const VkPipelineStageFlags waitStages[] {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1U;
    submitInfo.pWaitSemaphores = &frameSync.imageAvailable;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1U;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1U;
    submitInfo.pSignalSemaphores = &frameSync.renderFinished;

    if(vkQueueSubmit(m_context.graphics_queue(), 1U, &submitInfo, frameSync.inFlight) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to submit draw command buffer"};
    }

    VkPresentInfoKHR presentInfo {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1U;
    presentInfo.pWaitSemaphores = &frameSync.renderFinished;
    const VkSwapchainKHR swapchains[] {m_swapchain.handle()};
    presentInfo.swapchainCount = 1U;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    const VkResult presentResult = vkQueuePresentKHR(m_context.present_queue(), &presentInfo);

    if(presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
        m_framebufferResized = false;
        recreate_swapchain();
    } else if(presentResult != VK_SUCCESS) {
        throw std::runtime_error {"Failed to present swapchain image"};
    }

    m_currentFrame = (m_currentFrame + 1U) % maxFramesInFlight;
}

void VulkanApplication::recreate_swapchain() {
    int width {0};
    int height {0};
    glfwGetFramebufferSize(m_window.handle(), &width, &height);
    while(width == 0 || height == 0) {
        glfwWaitEvents();
        glfwGetFramebufferSize(m_window.handle(), &width, &height);
    }

    wait_for_device_idle();

    m_swapchain.recreate(m_context, m_window);
    m_renderPass = RenderPass::create(m_context, m_swapchain.image_format());
    m_pipeline.recreate(m_context, m_renderPass);
    m_framebuffers.recreate(m_context, m_swapchain, m_renderPass);
    const std::uint32_t commandBufferCount = static_cast<std::uint32_t>(m_framebuffers.size());
    m_commandAllocator.recreate(m_context, commandBufferCount);
    m_imagesInFlight.assign(commandBufferCount, VK_NULL_HANDLE);
    ImGui_ImplVulkan_SetMinImageCount(static_cast<std::uint32_t>(m_swapchain.image_views().size()));
}

void VulkanApplication::wait_for_device_idle() const {
    if(vkDeviceWaitIdle(m_context.device()) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to wait for device idle"};
    }
}

void VulkanApplication::init_imgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(m_window.handle(), true);

    const std::array<VkDescriptorPoolSize, 11U> poolSizes {
        VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_SAMPLER, 1000U},
        VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000U},
        VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000U},
        VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000U},
        VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000U},
        VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000U},
        VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000U},
        VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000U},
        VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000U},
        VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000U},
        VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000U}
    };

    VkDescriptorPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000U * static_cast<std::uint32_t>(poolSizes.size());
    poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    if(vkCreateDescriptorPool(m_context.device(), &poolInfo, nullptr, &m_imguiDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create ImGui descriptor pool"};
    }
    m_context.set_object_name(VK_OBJECT_TYPE_DESCRIPTOR_POOL, reinterpret_cast<std::uint64_t>(m_imguiDescriptorPool), "ImGui Descriptor Pool");

    ImGui_ImplVulkan_LoadFunctions(
        [](const char* functionName, void* userData) -> PFN_vkVoidFunction {
            const auto instance = reinterpret_cast<VkInstance>(userData);
            return vkGetInstanceProcAddr(instance, functionName);
        },
        reinterpret_cast<void*>(m_context.instance()));

    ImGui_ImplVulkan_InitInfo initInfo {};
    initInfo.Instance = m_context.instance();
    initInfo.PhysicalDevice = m_context.physical_device();
    initInfo.Device = m_context.device();
    initInfo.QueueFamily = m_context.graphics_queue_index();
    initInfo.Queue = m_context.graphics_queue();
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = m_imguiDescriptorPool;
    initInfo.Subpass = 0U;
    initInfo.MinImageCount = static_cast<std::uint32_t>(m_swapchain.image_views().size());
    initInfo.ImageCount = static_cast<std::uint32_t>(m_swapchain.image_views().size());
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.Allocator = nullptr;
    initInfo.CheckVkResultFn = nullptr;
    initInfo.RenderPass = m_renderPass.handle();
    initInfo.MinAllocationSize = 1024U * 1024U;

    if(!ImGui_ImplVulkan_Init(&initInfo)) {
        throw std::runtime_error {"Failed to initialise ImGui Vulkan backend"};
    }
}

void VulkanApplication::destroy_imgui() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if(m_imguiDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_context.device(), m_imguiDescriptorPool, nullptr);
        m_imguiDescriptorPool = VK_NULL_HANDLE;
    }
}

void VulkanApplication::begin_imgui_frame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void VulkanApplication::record_command_buffer(std::uint32_t imageIndex) {
    VkCommandBuffer commandBuffer = m_commandAllocator.buffers().at(imageIndex);
    if(vkResetCommandBuffer(commandBuffer, 0U) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to reset command buffer"};
    }

    VkCommandBufferBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to begin recording command buffer"};
    }

    VkClearValue clearValue {};
    clearValue.color = {{clearColour[0], clearColour[1], clearColour[2], clearColour[3]}};

    VkRenderPassBeginInfo renderPassInfo {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass.handle();
    renderPassInfo.framebuffer = m_framebuffers.handles().at(imageIndex);
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapchain.extent();
    renderPassInfo.clearValueCount = 1U;
    renderPassInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport {};
    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = static_cast<float>(m_swapchain.extent().width);
    viewport.height = static_cast<float>(m_swapchain.extent().height);
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;
    vkCmdSetViewport(commandBuffer, 0U, 1U, &viewport);

    VkRect2D scissor {};
    scissor.offset = {0, 0};
    scissor.extent = m_swapchain.extent();
    vkCmdSetScissor(commandBuffer, 0U, 1U, &scissor);

    m_context.begin_debug_label(commandBuffer, "Draw Triangle");
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.handle());
    const VkBuffer vertexBuffers[] {m_vertexBuffer.handle()};
    const VkDeviceSize offsets[] {0U};
    vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, vertexBuffers, offsets);
    vkCmdPushConstants(commandBuffer, m_pipeline.layout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0U, static_cast<std::uint32_t>(sizeof(glm::vec4)), &m_triangleColor);
    vkCmdDraw(commandBuffer, 3U, 1U, 0U, 0U);
    m_context.end_debug_label(commandBuffer);

    m_context.begin_debug_label(commandBuffer, "ImGui Overlay");
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    m_context.end_debug_label(commandBuffer);

    vkCmdEndRenderPass(commandBuffer);

    if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to record command buffer"};
    }
}

void VulkanApplication::update_timing(double deltaSeconds) {
    if(deltaSeconds <= 0.0) {
        return;
    }
    const double frameTimeMs = deltaSeconds * 1000.0;
    if(m_frameTimeMs <= 0.0) {
        m_frameTimeMs = frameTimeMs;
    } else {
        m_frameTimeMs = (timingSmoothingFactor * frameTimeMs) + ((1.0 - timingSmoothingFactor) * m_frameTimeMs);
    }
    m_fps = (m_frameTimeMs > 0.0) ? 1000.0 / m_frameTimeMs : 0.0;
}

} // namespace vulkano
