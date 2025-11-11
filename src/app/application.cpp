#include <vulkano/app/application.hpp>

#include <vulkano/app/camera.hpp>
#include <vulkano/app/camera_controller.hpp>
#include <vulkano/app/frame_resources.hpp>
#include <vulkano/app/glfw_library.hpp>
#include <vulkano/app/imgui_renderer.hpp>
#include <vulkano/app/material_buffer.hpp>
#include <vulkano/app/material_texture_cache.hpp>
#include <vulkano/app/light_buffer.hpp>
#include <vulkano/app/ssao.hpp>
#include <vulkano/app/scene_renderer.hpp>
#include <vulkano/app/vulkan_context.hpp>
#include <vulkano/app/window.hpp>
#include <vulkano/scene/material.hpp>
#include <vulkano/scene/light.hpp>
#include <vulkano/scene/mesh.hpp>

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

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

        scene::MaterialRegistry materialRegistry {};
        MaterialBuffer materialBuffer {context};
        MaterialTextureCache materialTextures {context};
        scene::LightRegistry lightRegistry {};
        LightBuffer lightBuffer {context};

        scene::Light fillLight {};
        fillLight.type = scene::LightType::Point;
        fillLight.position = glm::vec3 {2.0F, 1.5F, -1.0F};
        fillLight.range = 9.0F;
        fillLight.color = glm::vec3 {0.6F, 0.7F, 1.0F};
        fillLight.intensity = 3.5F;
        [[maybe_unused]] const scene::LightId fillId = lightRegistry.add_light(fillLight);

        scene::Material planeMaterial {};
        planeMaterial.properties.baseColor = glm::vec3 {0.7F, 0.7F, 0.7F};
        planeMaterial.properties.roughness = 0.9F;
        planeMaterial.textures.baseColorPath = "assets/textures/plane.png";
        planeMaterial.useBaseColorTexture = true;
        planeMaterial.useNormalTexture = true;
        planeMaterial.textures.normalPath = "assets/textures/metal_normal.png";
        const scene::MaterialId planeMaterialId = materialRegistry.add_material(planeMaterial);

        scene::Material cubeMaterial {};
        cubeMaterial.properties.baseColor = glm::vec3 {0.8F, 0.2F, 0.2F};
        cubeMaterial.properties.roughness = 0.4F;
        cubeMaterial.properties.metallic = 0.05F;
        cubeMaterial.textures.baseColorPath = "assets/textures/cube.png";
        cubeMaterial.textures.normalPath = "assets/textures/metal_normal.png";
        cubeMaterial.useBaseColorTexture = true;
        cubeMaterial.useNormalTexture = true;
        const scene::MaterialId cubeMaterialId = materialRegistry.add_material(cubeMaterial);

        scene::Material sphereMaterial {};
        sphereMaterial.properties.baseColor = glm::vec3 {0.2F, 0.4F, 0.85F};
        sphereMaterial.properties.roughness = 0.2F;
        sphereMaterial.properties.metallic = 0.6F;
        sphereMaterial.textures.baseColorPath = "assets/textures/sphere.png";
        sphereMaterial.useBaseColorTexture = true;
        sphereMaterial.useNormalTexture = true;
        sphereMaterial.textures.normalPath = "assets/textures/metal_normal.png";
        const scene::MaterialId sphereMaterialId = materialRegistry.add_material(sphereMaterial);

        std::vector<SceneRenderer::SceneMesh> sceneMeshes {
            SceneRenderer::SceneMesh {
                .mesh = vulkano::scene::MeshFactory::create_plane(10.0F, glm::vec3 {0.7F, 0.7F, 0.7F}),
                .model = glm::mat4(1.0F),
                .material = planeMaterialId
            },
            SceneRenderer::SceneMesh {
                .mesh = vulkano::scene::MeshFactory::create_cube(1.0F, glm::vec3 {0.8F, 0.2F, 0.2F}),
                .model = glm::translate(glm::mat4(1.0F), glm::vec3 {-1.5F, 0.5F, 0.0F}),
                .material = cubeMaterialId
            },
            SceneRenderer::SceneMesh {
                .mesh = vulkano::scene::MeshFactory::create_uv_sphere(0.5F, 32U, 16U, glm::vec3 {0.2F, 0.4F, 0.85F}),
                .model = glm::translate(glm::mat4(1.0F), glm::vec3 {1.5F, 0.5F, 0.0F}),
                .material = sphereMaterialId
            }
        };

        materialTextures.rebuild(materialRegistry);
        materialBuffer.update(materialRegistry, materialTextures.handles());
        lightBuffer.update(lightRegistry);
        bool materialsDirty = false;
        bool lightsDirty = false;
        bool showLightDebug = true;

        SSAOSampleGenerator ssaoGenerator {};
        SSAOGpuResources ssaoResources {context, ssaoGenerator, 64U, 4U};
        auto ssaoComposite = std::make_unique<SSAOCompositeDescriptors>(context);

        auto renderer = std::make_unique<SceneRenderer>(context, window, ssaoComposite->layout());
        renderer->set_scene(sceneMeshes);
        renderer->set_material_resources(materialBuffer, materialTextures);
        renderer->set_light_resources(lightBuffer, lightRegistry);
        renderer->set_show_light_debug(showLightDebug);

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
        float ssaoDepthRange = 0.2F;
        float ssaoDistanceRange = 0.5F;
        float ssaoNormalEpsilon = 0.05F;
        ssaoDescriptors->set_camera_parameters(camera.projection_matrix(), glm::inverse(camera.projection_matrix()),
            context.swapchain_extent(), ssaoRadius, ssaoBias, ssaoResources.noise_dimension(), ssaoAngleCos,
            ssaoDepthRange, ssaoDistanceRange, ssaoNormalEpsilon);

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
            renderer->set_material_resources(materialBuffer, materialTextures);
            renderer->set_light_resources(lightBuffer, lightRegistry);
            renderer->set_show_light_debug(showLightDebug);
            ssaoDescriptors->update_gbuffer_views(renderer->normal_image_view(), renderer->linear_depth_image_view());
            ssaoPass->resize(context, context.swapchain_extent());
            ssaoBlurPass->resize(context, context.swapchain_extent());
            ssaoBlurPass->set_depth_view(renderer->linear_depth_image_view());
            ssaoBlurPass->set_occlusion_view(ssaoPass->occlusion_view());
            ssaoComposite->update_occlusion_view(ssaoBlurPass->blurred_view());
            ssaoDescriptors->set_camera_parameters(camera.projection_matrix(), glm::inverse(camera.projection_matrix()),
                context.swapchain_extent(), ssaoRadius, ssaoBias, ssaoResources.noise_dimension(), ssaoAngleCos,
                ssaoDepthRange, ssaoDistanceRange, ssaoNormalEpsilon);
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
                ImGui::SliderFloat("Occlusion Strength", &ssaoStrength, 0.0F, 2.0F);
                ImGui::SliderFloat("Base Ambient Light", &ssaoBaseAmbient, 0.0F, 1.0F);

                if (ImGui::CollapsingHeader("Sampling", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::SliderFloat("Sampling Radius (m)##SSAORadius", &ssaoRadius, 0.1F, 4.0F);
                    ImGui::SliderFloat("Sampling Bias (m)##SSAOBias", &ssaoBias, 0.0F, 0.1F);
                    ImGui::SliderFloat("Angle Threshold (cos)##SSAOCos", &ssaoAngleCos, 0.0F, 1.0F);
                    ImGui::SliderFloat("Depth Influence Range (m)##SSAODepthRange", &ssaoDepthRange, 0.01F, 1.0F);
                    ImGui::SliderFloat("Distance Influence Range (m)##SSAODistanceRange", &ssaoDistanceRange, 0.05F, 3.0F);
                    ImGui::SliderFloat("Normal Cutoff##SSAONormalEps", &ssaoNormalEpsilon, 0.0F, 0.5F);
                }

                if (ImGui::CollapsingHeader("Blur", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::SliderFloat("Blur Radius (px)##SSAOBlurRadius", &ssaoBlurRadius, 0.5F, 6.0F);
                    ImGui::SliderFloat("Blur Depth Sensitivity##SSAOBlurDepthSigma", &ssaoBlurDepthSigma, 0.01F, 1.0F);
                }

                ImGui::Checkbox("Show Raw Occlusion", &ssaoDebugView);
            }
            ImGui::End();

            if (ImGui::Begin("Lighting")) {
                if (ImGui::Checkbox("Show Light Gizmo", &showLightDebug)) {
                    renderer->set_show_light_debug(showLightDebug);
                }

                if (ImGui::Button("Add Directional Light")) {
                    scene::Light light {};
                    light.type = scene::LightType::Directional;
                    light.direction = glm::normalize(glm::vec3 {-0.2F, -1.0F, -0.1F});
                    light.color = glm::vec3 {0.8F, 0.85F, 1.0F};
                    light.intensity = 1.5F;
                    [[maybe_unused]] const scene::LightId newDirectional = lightRegistry.add_light(light);
                    lightsDirty = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Add Point Light")) {
                    scene::Light light {};
                    light.type = scene::LightType::Point;
                    light.position = glm::vec3 {1.0F, 1.5F, 0.5F};
                    light.range = 8.0F;
                    light.color = glm::vec3 {1.0F, 0.9F, 0.75F};
                    light.intensity = 3.0F;
                    [[maybe_unused]] const scene::LightId newPoint = lightRegistry.add_light(light);
                    lightsDirty = true;
                }

                std::size_t lightIndex {0U};
                while (lightIndex < lightRegistry.size()) {
                    const scene::LightId id {static_cast<std::uint32_t>(lightIndex)};
                    scene::Light& editableLight = lightRegistry.light(id);
                    ImGui::PushID(static_cast<int>(lightIndex));
                    const std::string label = "Light " + std::to_string(lightIndex);

                    if (ImGui::TreeNode(label.c_str())) {
                        int typeIndex = static_cast<int>(editableLight.type);
                        const char* typeItems[] = {"Directional", "Point"};
                        if (ImGui::Combo("Type", &typeIndex, typeItems, IM_ARRAYSIZE(typeItems))) {
                            editableLight.type = static_cast<scene::LightType>(typeIndex);
                            lightsDirty = true;
                        }

                        if (editableLight.type == scene::LightType::Directional) {
                            glm::vec3 direction = editableLight.direction;
                            if (ImGui::SliderFloat3("Direction", glm::value_ptr(direction), -1.0F, 1.0F)) {
                                editableLight.direction = direction;
                                lightsDirty = true;
                            }
                        } else {
                            glm::vec3 position = editableLight.position;
                            if (ImGui::DragFloat3("Position", glm::value_ptr(position), 0.1F, -20.0F, 20.0F)) {
                                editableLight.position = position;
                                lightsDirty = true;
                            }
                            float range = editableLight.range;
                            if (ImGui::SliderFloat("Range", &range, 0.1F, 50.0F)) {
                                editableLight.range = range;
                                lightsDirty = true;
                            }
                        }

                        glm::vec3 color = editableLight.color;
                        if (ImGui::ColorEdit3("Color", glm::value_ptr(color))) {
                            editableLight.color = color;
                            lightsDirty = true;
                        }

                        float intensity = editableLight.intensity;
                        if (ImGui::SliderFloat("Intensity", &intensity, 0.0F, 10.0F)) {
                            editableLight.intensity = intensity;
                            lightsDirty = true;
                        }

                        if (lightIndex > 0U) {
                            if (ImGui::Button("Remove")) {
                                lightRegistry.remove_light(id);
                                lightsDirty = true;
                                ImGui::TreePop();
                                ImGui::PopID();
                                continue;
                            }
                        }

                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                    ++lightIndex;
                }
            }
            ImGui::End();

            if (ImGui::Begin("Materials")) {
                for (std::size_t index {0U}; index < materialRegistry.size(); ++index) {
                    const scene::MaterialId id {static_cast<std::uint32_t>(index)};
                    scene::Material& editable = materialRegistry.material(id);
                    const std::string label = "Material " + std::to_string(index);

                    if (ImGui::TreeNode(label.c_str())) {
                        glm::vec3 baseColor = editable.properties.baseColor;
                        if (ImGui::ColorEdit3("Base Color", glm::value_ptr(baseColor))) {
                            editable.properties.baseColor = baseColor;
                            materialsDirty = true;
                        }
                        ImGui::ColorButton("##BasePreview", ImVec4(baseColor.x, baseColor.y, baseColor.z, 1.0F), 0,
                            ImVec2(32.0F, 32.0F));

                        ImGui::SameLine();
                        ImGui::TextUnformatted("Base Color Preview");

                        float metallic = editable.properties.metallic;
                        if (ImGui::SliderFloat("Metallic", &metallic, 0.0F, 1.0F)) {
                            editable.properties.metallic = metallic;
                            materialsDirty = true;
                        }

                        float roughness = editable.properties.roughness;
                        if (ImGui::SliderFloat("Roughness", &roughness, 0.0F, 1.0F)) {
                            editable.properties.roughness = roughness;
                            materialsDirty = true;
                        }

                        glm::vec3 emissiveColor = editable.properties.emissive;
                        if (ImGui::ColorEdit3("Emissive Color", glm::value_ptr(emissiveColor))) {
                            editable.properties.emissive = emissiveColor;
                            materialsDirty = true;
                        }
                        float emissiveIntensity = editable.properties.emissiveIntensity;
                        if (ImGui::SliderFloat("Emissive Intensity", &emissiveIntensity, 0.0F, 10.0F)) {
                            editable.properties.emissiveIntensity = emissiveIntensity;
                            materialsDirty = true;
                        }

                        ImGui::TextUnformatted("Base texture path:");
                        ImGui::TextWrapped("%s", editable.textures.baseColorPath.empty() ? "<none>"
                            : editable.textures.baseColorPath.c_str());

                        bool useNormal = editable.useNormalTexture;
                        if (ImGui::Checkbox("Use Normal Map", &useNormal)) {
                            editable.useNormalTexture = useNormal;
                            materialsDirty = true;
                        }
                        ImGui::TextWrapped("Normal texture: %s",
                            editable.textures.normalPath.empty() ? "<none>"
                                                                 : editable.textures.normalPath.c_str());
                        ImGui::ColorButton("##NormalPreview", ImVec4(0.5F, 0.5F, 1.0F, 1.0F), 0, ImVec2(24.0F, 24.0F));
                        ImGui::SameLine();
                        ImGui::TextUnformatted("Normal Map Preview");
                        float normalStrength = editable.properties.normalStrength;
                        if (ImGui::SliderFloat("Normal Strength", &normalStrength, 0.0F, 1.0F)) {
                            editable.properties.normalStrength = normalStrength;
                            materialsDirty = true;
                        }

                        if (!editable.textures.metallicRoughnessPath.empty()) {
                            bool useMr = editable.useMetallicRoughnessTexture;
                            if (ImGui::Checkbox("Use Metallic/Roughness Map", &useMr)) {
                                editable.useMetallicRoughnessTexture = useMr;
                                materialsDirty = true;
                            }
                            ImGui::TextWrapped("MR texture: %s", editable.textures.metallicRoughnessPath.c_str());
                            ImGui::ColorButton("##MRPreview",
                                ImVec4(editable.properties.metallic, editable.properties.roughness, 0.0F, 1.0F), 0,
                                ImVec2(24.0F, 24.0F));
                            ImGui::SameLine();
                            ImGui::TextUnformatted("Metallic/Roughness Preview");
                        }

                        if (!editable.textures.ambientOcclusionPath.empty()) {
                            bool useAo = editable.useAmbientOcclusionTexture;
                            if (ImGui::Checkbox("Use Ambient Occlusion Map", &useAo)) {
                                editable.useAmbientOcclusionTexture = useAo;
                                materialsDirty = true;
                            }
                            ImGui::TextWrapped("AO texture: %s", editable.textures.ambientOcclusionPath.c_str());
                            ImGui::ColorButton("##AOPreview",
                                ImVec4(editable.properties.ambientOcclusion, editable.properties.ambientOcclusion,
                                    editable.properties.ambientOcclusion, 1.0F), 0, ImVec2(24.0F, 24.0F));
                            ImGui::SameLine();
                            ImGui::TextUnformatted("Ambient Occlusion Preview");
                        }
                        ImGui::TreePop();
                    }
                }
            }
            ImGui::End();
            float effectiveStrength = ssaoEnabled ? ssaoStrength : 0.0F;
            ssaoComposite->set_config(effectiveStrength, ssaoBaseAmbient, ssaoDebugView);
            ssaoDescriptors->set_camera_parameters(camera.projection_matrix(), glm::inverse(camera.projection_matrix()),
                context.swapchain_extent(), ssaoRadius, ssaoBias, ssaoResources.noise_dimension(), ssaoAngleCos,
                ssaoDepthRange, ssaoDistanceRange, ssaoNormalEpsilon);
            ssaoBlurPass->set_parameters(ssaoBlurRadius, ssaoBlurDepthSigma);
            // Descriptor already points at blurred occlusion; no per-frame update needed
            imgui->end_frame();

            if (materialsDirty) {
                context.wait_idle();
                materialTextures.rebuild(materialRegistry);
                materialBuffer.update(materialRegistry, materialTextures.handles());
                renderer->set_material_resources(materialBuffer, materialTextures);
                materialsDirty = false;
            }

           if (lightsDirty) {
                context.wait_idle();
                lightBuffer.update(lightRegistry);
                renderer->set_light_resources(lightBuffer, lightRegistry);
                renderer->set_show_light_debug(showLightDebug);
                lightsDirty = false;
            }

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
            renderer->record_command_buffer(commandBuffer, imageIndex, camera.view_matrix(), camera.projection_matrix(),
                camera.position(), overlayRecorder, ssaoComposite->descriptor_set());
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
