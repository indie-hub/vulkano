#include <vulkano/app/application.hpp>

#include <vulkano/app/frame_resources.hpp>
#include <vulkano/app/imgui_renderer.hpp>
#include <vulkano/app/glfw_library.hpp>
#include <vulkano/app/scene_renderer.hpp>
#include <vulkano/scene/mesh.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkano/app/vulkan_context.hpp>
#include <vulkano/app/window.hpp>

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <chrono>

namespace vulkano::app {
namespace {
[[nodiscard]] bool should_skip_frame(VkResult result) noexcept {
    return result == VK_ERROR_OUT_OF_DATE_KHR;
}
}

int Application::run() noexcept {
    try {
        GlfwLibrary glfw {};
        glfw.ensure_vulkan_support();

        Window window {"Vulkano Renderer", 1280U, 720U};
        VulkanContext context {window};
        SceneRenderer renderer {context, window};
        const SceneRenderer::SceneMesh planeMesh {
            .mesh = vulkano::scene::MeshFactory::create_plane(10.0F, glm::vec3 {0.7F, 0.7F, 0.7F}),
            .model = glm::mat4(1.0F)
        };
        const SceneRenderer::SceneMesh cubeMesh {
            .mesh = vulkano::scene::MeshFactory::create_cube(1.0F, glm::vec3 {0.8F, 0.2F, 0.2F}),
            .model = glm::translate(glm::mat4(1.0F), glm::vec3 {-1.5F, 0.5F, 0.0F})
        };
        const SceneRenderer::SceneMesh sphereMesh {
            .mesh = vulkano::scene::MeshFactory::create_uv_sphere(0.5F, 32U, 16U, glm::vec3 {0.2F, 0.4F, 0.85F}),
            .model = glm::translate(glm::mat4(1.0F), glm::vec3 {1.5F, 0.5F, 0.0F})
        };
        renderer.set_scene({planeMesh, cubeMesh, sphereMesh});
        ImGuiRenderer imgui {context, window, renderer.render_pass()};
        FrameResources frameResources {context};

        auto previousFrameTime = std::chrono::steady_clock::now();

        const char* maxFramesEnv = std::getenv("VULKANO_MAX_FRAMES");
        std::size_t maxFrames = std::numeric_limits<std::size_t>::max();
        if (maxFramesEnv != nullptr) {
            char* endPtr = nullptr;
            const unsigned long long parsed = std::strtoull(maxFramesEnv, &endPtr, 10);
            if (endPtr != maxFramesEnv) {
                maxFrames = static_cast<std::size_t>(parsed);
            }
        }

        std::size_t currentFrame {0U};
        std::size_t frameCount {0U};
        while (!window.should_close()) {
            const auto frameStart = std::chrono::steady_clock::now();
            const float deltaSeconds = std::chrono::duration<float>(frameStart - previousFrameTime).count();
            previousFrameTime = frameStart;

            window.poll_events();

            imgui.begin_frame();
            imgui.update_metrics(deltaSeconds);
            imgui.draw_overlay();
            imgui.end_frame();

            const VkFence inFlightFence = frameResources.in_flight_fence(currentFrame);
            if (vkWaitForFences(context.device(), 1U, &inFlightFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
                throw std::runtime_error {"Failed to wait for in-flight fence"};
            }

            std::uint32_t imageIndex {0U};
            const VkResult acquireResult = vkAcquireNextImageKHR(context.device(), context.swapchain(), UINT64_MAX,
                frameResources.image_available_semaphore(currentFrame), VK_NULL_HANDLE, &imageIndex);

            if (should_skip_frame(acquireResult)) {
                continue;
            }
            if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
                throw std::runtime_error {"Failed to acquire swap chain image"};
            }

            const VkFence imageFence = frameResources.image_in_flight(imageIndex);
            if (imageFence != VK_NULL_HANDLE) {
                if (vkWaitForFences(context.device(), 1U, &imageFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
                    throw std::runtime_error {"Failed to wait for image fence"};
                }
            }

            frameResources.set_image_in_flight(imageIndex, frameResources.in_flight_fence(currentFrame));
            frameResources.reset_fence(currentFrame);

            VkCommandBuffer commandBuffer = frameResources.command_buffers()[imageIndex];
            if (vkResetCommandBuffer(commandBuffer, 0U) != VK_SUCCESS) {
                throw std::runtime_error {"Failed to reset command buffer"};
            }
            const SceneRenderer::CommandRecorder overlayRecorder {[&imgui](VkCommandBuffer buffer) {
                imgui.render(buffer);
            }};
            renderer.record_command_buffer(commandBuffer, imageIndex, overlayRecorder);

            const VkSemaphore waitSemaphores[] = {frameResources.image_available_semaphore(currentFrame)};
            const VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            const VkSemaphore signalSemaphores[] = {frameResources.render_finished_semaphore(imageIndex)};

            VkSubmitInfo submitInfo {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.waitSemaphoreCount = 1U;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1U;
            submitInfo.pCommandBuffers = &commandBuffer;
            submitInfo.signalSemaphoreCount = 1U;
            submitInfo.pSignalSemaphores = signalSemaphores;

            if (vkQueueSubmit(context.graphics_queue(), 1U, &submitInfo, frameResources.in_flight_fence(currentFrame))
                != VK_SUCCESS) {
                throw std::runtime_error {"Failed to submit draw command buffer"};
            }

            const VkSwapchainKHR swapchains[] = {context.swapchain()};
            VkPresentInfoKHR presentInfo {};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1U;
            presentInfo.pWaitSemaphores = signalSemaphores;
            presentInfo.swapchainCount = 1U;
            presentInfo.pSwapchains = swapchains;
            presentInfo.pImageIndices = &imageIndex;

            const VkResult presentResult = vkQueuePresentKHR(context.present_queue(), &presentInfo);
            if (should_skip_frame(presentResult)) {
                continue;
            }
            if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR) {
                throw std::runtime_error {"Failed to present swap chain image"};
            }

            ++frameCount;
            if (frameCount >= maxFrames) {
                break;
            }

            currentFrame = (currentFrame + 1U) % frameResources.frames_in_flight();
        }

        context.wait_idle();
        return EXIT_SUCCESS;
    } catch (const std::exception& exception) {
        std::cerr << "Application error: " << exception.what() << '\n';
    } catch (...) {
        std::cerr << "Application error: unknown exception" << '\n';
    }

    return EXIT_FAILURE;
}
} // namespace vulkano::app
