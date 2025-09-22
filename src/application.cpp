#include <vulkano/application.hpp>

#include <span>
#include <stdexcept>
#include <array>
#include <limits>

#include <GLFW/glfw3.h>

namespace {
    constexpr std::uint32_t maxFramesInFlight {2U};
    constexpr std::array<float, 4U> clearColour {0.0F, 0.0F, 0.0F, 1.0F};
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
    , m_syncManager {SyncManager::create(m_context, maxFramesInFlight)} {
    const auto vertices = default_triangle_vertices();
    m_vertexBuffer = Buffer::create_vertex_buffer(m_context, std::span<const Vertex>(vertices.data(), vertices.size()));
    m_pipeline = GraphicsPipeline::create(m_context, m_renderPass);
    m_imagesInFlight.resize(m_swapchain.image_views().size(), VK_NULL_HANDLE);

    register_callbacks();
    rebuild_command_buffers();
}

VulkanApplication::~VulkanApplication() {
    wait_for_device_idle();
}

void VulkanApplication::run() {
    while(!m_window.should_close()) {
        glfwPollEvents();
        draw_frame();
    }
    wait_for_device_idle();
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

    rebuild_command_buffers();
}

void VulkanApplication::rebuild_command_buffers() {
    m_commandAllocator.reset();
    const auto& commandBuffers = m_commandAllocator.buffers();
    const auto& framebuffers = m_framebuffers.handles();
    const VkExtent2D extent = m_swapchain.extent();

    for(std::size_t index {0U}; index < commandBuffers.size(); ++index) {
        const VkCommandBuffer commandBuffer = commandBuffers.at(index);

        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0U;
        beginInfo.pInheritanceInfo = nullptr;

        if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to begin recording command buffer"};
        }

        VkClearValue clearValue {};
        clearValue.color = {{clearColour[0], clearColour[1], clearColour[2], clearColour[3]}};

        VkRenderPassBeginInfo renderPassInfo {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_renderPass.handle();
        renderPassInfo.framebuffer = framebuffers.at(index);
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = extent;
        renderPassInfo.clearValueCount = 1U;
        renderPassInfo.pClearValues = &clearValue;

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport {};
        viewport.x = 0.0F;
        viewport.y = 0.0F;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0F;
        viewport.maxDepth = 1.0F;
        vkCmdSetViewport(commandBuffer, 0U, 1U, &viewport);

        VkRect2D scissor {};
        scissor.offset = {0, 0};
        scissor.extent = extent;
        vkCmdSetScissor(commandBuffer, 0U, 1U, &scissor);

        m_context.begin_debug_label(commandBuffer, "Draw Triangle");

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.handle());

        const VkBuffer vertexBuffers[] {m_vertexBuffer.handle()};
        const VkDeviceSize offsets[] {0U};
        vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, vertexBuffers, offsets);
        vkCmdPushConstants(commandBuffer, m_pipeline.layout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0U, static_cast<std::uint32_t>(sizeof(glm::vec4)), &m_triangleColor);
        vkCmdDraw(commandBuffer, 3U, 1U, 0U, 0U);

        m_context.end_debug_label(commandBuffer);

        vkCmdEndRenderPass(commandBuffer);

        if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to record command buffer"};
        }
    }
}

void VulkanApplication::wait_for_device_idle() const {
    if(vkDeviceWaitIdle(m_context.device()) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to wait for device idle"};
    }
}

} // namespace vulkano
