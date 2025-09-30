#include <vulkano/app/application.hpp>

#include <vulkano/app/camera.hpp>
#include <vulkano/app/camera_controller.hpp>
#include <vulkano/app/frame_resources.hpp>
#include <vulkano/app/glfw_library.hpp>
#include <vulkano/app/imgui_renderer.hpp>
#include <vulkano/app/ssao.hpp>
#include <vulkano/app/scene_renderer.hpp>
#include <vulkano/app/vulkan_context.hpp>
#include <vulkano/app/window.hpp>
#include <vulkano/scene/mesh.hpp>

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace vulkano::app {
int Application::run() noexcept {
    try {
        GlfwLibrary glfw {};
        glfw.ensure_vulkan_support();

        Window window {"Vulkano Renderer", 1280U, 720U};
        VulkanContext context {window};

        const VkExtent2D swapExtent = context.swapchain_extent();
        const float swapAspect = swapExtent.height == 0U ? 1.0F : static_cast<float>(swapExtent.width) / static_cast<float>(swapExtent.height);
        Camera camera {glm::vec3 {0.0F, 1.5F, 5.0F}, -90.0F, -15.0F, swapAspect};
        CameraController cameraController {camera, window};
        cameraController.set_move_speed(6.0F);
        cameraController.set_mouse_sensitivity(0.1F);
        std::vector<SceneRenderer::SceneMesh> sceneMeshes {
            SceneRenderer::SceneMesh {
                .mesh = vulkano::scene::MeshFactory::create_plane(10.0F, glm::vec3 {0.7F, 0.7F, 0.7F}),
                .model = glm::mat4(1.0F)
            },
            SceneRenderer::SceneMesh {
                .mesh = vulkano::scene::MeshFactory::create_cube(1.0F, glm::vec3 {0.8F, 0.2F, 0.2F}),
                .model = glm::translate(glm::mat4(1.0F), glm::vec3 {-1.5F, 0.5F, 0.0F})
            },
            SceneRenderer::SceneMesh {
                .mesh = vulkano::scene::MeshFactory::create_uv_sphere(0.5F, 32U, 16U, glm::vec3 {0.2F, 0.4F, 0.85F}),
                .model = glm::translate(glm::mat4(1.0F), glm::vec3 {1.5F, 0.5F, 0.0F})
            }
        };

        SSAOSampleGenerator ssaoGenerator {};
        SSAOGpuResources ssaoResources {context, ssaoGenerator, 64U, 4U};
        auto ssaoComposite = std::make_unique<SSAOCompositeDescriptors>(context);

        auto renderer = std::make_unique<SceneRenderer>(context, window, ssaoComposite->layout());
        renderer->set_scene(sceneMeshes);

        auto ssaoDescriptors = std::make_unique<SSAODescriptors>(context, ssaoResources,
            renderer->normal_image_view(), renderer->linear_depth_image_view());
        auto ssaoPass = std::make_unique<SSAOPass>(context, ssaoDescriptors->layout(), context.swapchain_extent());
        auto ssaoBlurPass = std::make_unique<SSAOBlurPass>(context, context.swapchain_extent());
        ssaoBlurPass->set_depth_view(renderer->linear_depth_image_view());
        ssaoBlurPass->set_occlusion_view(ssaoPass->occlusion_view());
        float ssaoBlurRadius = 2.0F;
        float ssaoBlurDepthSigma = 0.1F;
        ssaoBlurPass->set_parameters(ssaoBlurRadius, ssaoBlurDepthSigma);
        ssaoComposite->update_occlusion_view(ssaoBlurPass->blurred_view());
        float ssaoRadius = 0.75F;
        float ssaoBias = 0.025F;
        float ssaoAngleCos = 0.3F;
        float ssaoDepthFalloff = 2.0F;
        float ssaoDistanceFalloff = 1.0F;
        float ssaoNormalEpsilon = 0.05F;
        ssaoDescriptors->set_camera_parameters(camera.projection_matrix(), glm::inverse(camera.projection_matrix()),
            context.swapchain_extent(), ssaoRadius, ssaoBias, ssaoResources.noise_dimension(), ssaoAngleCos,
            ssaoDepthFalloff, ssaoDistanceFalloff, ssaoNormalEpsilon);

        bool ssaoEnabled = true;
        float ssaoStrength = 1.0F;
        float ssaoBaseAmbient = 0.2F;
        bool ssaoDebugView = false;
        ssaoComposite->set_config(ssaoStrength, ssaoBaseAmbient, ssaoDebugView);

        auto imgui = std::make_unique<ImGuiRenderer>(context, window, renderer->render_pass());
        auto frameResources = std::make_unique<FrameResources>(context);

        auto recreateSwapchain = [&]() {
            context.wait_idle();

            VkExtent2D extent = window.framebuffer_extent();
            while (extent.width == 0U || extent.height == 0U) {
                glfwWaitEvents();
                extent = window.framebuffer_extent();
            }

            frameResources.reset();
            imgui.reset();
            renderer.reset();
            cameraController.reset();

            context.recreate_swapchain(window);

            const VkExtent2D newExtent = context.swapchain_extent();
            if (newExtent.height != 0U) {
                camera.set_aspect_ratio(static_cast<float>(newExtent.width) / static_cast<float>(newExtent.height));
            }

            renderer = std::make_unique<SceneRenderer>(context, window, ssaoComposite->layout());
            renderer->set_scene(sceneMeshes);
            ssaoDescriptors->update_gbuffer_views(renderer->normal_image_view(), renderer->linear_depth_image_view());
            ssaoPass->resize(context, context.swapchain_extent());
            ssaoBlurPass->resize(context, context.swapchain_extent());
            ssaoBlurPass->set_depth_view(renderer->linear_depth_image_view());
            ssaoBlurPass->set_occlusion_view(ssaoPass->occlusion_view());
            ssaoComposite->update_occlusion_view(ssaoBlurPass->blurred_view());
            ssaoDescriptors->set_camera_parameters(camera.projection_matrix(), glm::inverse(camera.projection_matrix()),
                context.swapchain_extent(), ssaoRadius, ssaoBias, ssaoResources.noise_dimension(), ssaoAngleCos,
                ssaoDepthFalloff, ssaoDistanceFalloff, ssaoNormalEpsilon);
            ssaoBlurPass->set_parameters(ssaoBlurRadius, ssaoBlurDepthSigma);

            imgui = std::make_unique<ImGuiRenderer>(context, window, renderer->render_pass());
            frameResources = std::make_unique<FrameResources>(context);

            window.clear_framebuffer_resized();
        };

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

            if (window.framebuffer_resized()) {
                recreateSwapchain();
                continue;
            }

            cameraController.update(deltaSeconds);

            // camera parameters will be updated after UI to capture new SSAO tuning values

            imgui->begin_frame();
            imgui->update_metrics(deltaSeconds);
            imgui->draw_overlay();
            if (ImGui::Begin("SSAO")) {
                ImGui::Checkbox("Enable", &ssaoEnabled);
                ImGui::SliderFloat("Strength", &ssaoStrength, 0.0F, 2.0F);
                ImGui::SliderFloat("Base Ambient", &ssaoBaseAmbient, 0.0F, 1.0F);
                ImGui::SliderFloat("SSAORadius", &ssaoRadius, 0.1F, 4.0F);
                ImGui::SliderFloat("SSAOBias", &ssaoBias, 0.0F, 0.1F);
                ImGui::SliderFloat("SSAOCos", &ssaoAngleCos, 0.0F, 1.0F);
                ImGui::SliderFloat("SSAODepthFalloff", &ssaoDepthFalloff, 0.1F, 8.0F);
                ImGui::SliderFloat("SSAODistanceFalloff", &ssaoDistanceFalloff, 0.1F, 8.0F);
                ImGui::SliderFloat("SSAONormalEps", &ssaoNormalEpsilon, 0.0F, 0.5F);
                ImGui::SliderFloat("Blur Radius", &ssaoBlurRadius, 0.5F, 6.0F);
                ImGui::SliderFloat("Blur Depth Sigma", &ssaoBlurDepthSigma, 0.01F, 1.0F);
                ImGui::Checkbox("Debug Occlusion", &ssaoDebugView);
            }
            ImGui::End();
            float effectiveStrength = ssaoEnabled ? ssaoStrength : 0.0F;
            ssaoComposite->set_config(effectiveStrength, ssaoBaseAmbient, ssaoDebugView);
            ssaoDescriptors->set_camera_parameters(camera.projection_matrix(), glm::inverse(camera.projection_matrix()),
                context.swapchain_extent(), ssaoRadius, ssaoBias, ssaoResources.noise_dimension(), ssaoAngleCos,
                ssaoDepthFalloff, ssaoDistanceFalloff, ssaoNormalEpsilon);
            ssaoBlurPass->set_parameters(ssaoBlurRadius, ssaoBlurDepthSigma);
            // Descriptor already points at blurred occlusion; no per-frame update needed
            imgui->end_frame();

            const VkFence inFlightFence = frameResources->in_flight_fence(currentFrame);
            if (vkWaitForFences(context.device(), 1U, &inFlightFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
                throw std::runtime_error {"Failed to wait for in-flight fence"};
            }

            std::uint32_t imageIndex {0U};
            const VkResult acquireResult = vkAcquireNextImageKHR(context.device(), context.swapchain(), UINT64_MAX,
                frameResources->image_available_semaphore(currentFrame), VK_NULL_HANDLE, &imageIndex);

            if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
                recreateSwapchain();
                continue;
            }
            if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
                throw std::runtime_error {"Failed to acquire swap chain image"};
            }
            const bool swapchainSuboptimal = acquireResult == VK_SUBOPTIMAL_KHR;

            const VkFence imageFence = frameResources->image_in_flight(imageIndex);
            if (imageFence != VK_NULL_HANDLE) {
                if (vkWaitForFences(context.device(), 1U, &imageFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
                    throw std::runtime_error {"Failed to wait for image fence"};
                }
            }

            frameResources->set_image_in_flight(imageIndex, frameResources->in_flight_fence(currentFrame));
            frameResources->reset_fence(currentFrame);

            VkCommandBuffer commandBuffer = frameResources->command_buffers()[imageIndex];
            if (vkResetCommandBuffer(commandBuffer, 0U) != VK_SUCCESS) {
                throw std::runtime_error {"Failed to reset command buffer"};
            }
            const SceneRenderer::CommandRecorder overlayRecorder {[&imgui](VkCommandBuffer buffer) {
                imgui->render(buffer);
            }};
            renderer->record_command_buffer(commandBuffer, imageIndex, camera.view_matrix(), camera.projection_matrix(), overlayRecorder, ssaoComposite->descriptor_set());
            ssaoPass->record(commandBuffer, *ssaoDescriptors);
            ssaoBlurPass->record(commandBuffer, ssaoPass->occlusion_view());

            if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
                throw std::runtime_error {"Failed to record command buffer"};
            }

            const VkSemaphore waitSemaphores[] = {frameResources->image_available_semaphore(currentFrame)};
            const VkPipelineStageFlags waitStages[] = {
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
            };
            const VkSemaphore signalSemaphores[] = {frameResources->render_finished_semaphore(imageIndex)};

            VkSubmitInfo submitInfo {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.waitSemaphoreCount = 1U;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1U;
            submitInfo.pCommandBuffers = &commandBuffer;
            submitInfo.signalSemaphoreCount = 1U;
            submitInfo.pSignalSemaphores = signalSemaphores;

            if (vkQueueSubmit(context.graphics_queue(), 1U, &submitInfo, frameResources->in_flight_fence(currentFrame))
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
            if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || swapchainSuboptimal
                || window.framebuffer_resized()) {
                recreateSwapchain();
                continue;
            }
            if (presentResult != VK_SUCCESS) {
                throw std::runtime_error {"Failed to present swap chain image"};
            }

            ++frameCount;
            if (frameCount >= maxFrames) {
                break;
            }

            currentFrame = (currentFrame + 1U) % frameResources->frames_in_flight();
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
