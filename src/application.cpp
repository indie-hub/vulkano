#include <vulkano/application.hpp>
#include <vulkano/camera_math.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {
    constexpr std::uint32_t maxFramesInFlight {2U};
    constexpr std::array<float, 4U> clearColour {0.0F, 0.0F, 0.0F, 1.0F};
    constexpr std::uint32_t maxCascades {4U};
    static_assert(maxCascades <= vulkano::ShadowMap::maxLayers, "Cascade count exceeds shadow map layer capacity");
    constexpr float timingSmoothingFactor {0.1F};
    constexpr float cameraBaseSpeed {4.0F};
    constexpr float cameraSprintMultiplier {2.5F};
    constexpr float mouseSensitivity {0.0025F};
    constexpr float pitchLimitRadians {glm::radians(89.0F)};
    constexpr float minFovRadians {glm::radians(35.0F)};
    constexpr float maxFovRadians {glm::radians(90.0F)};
    constexpr float fovScrollStep {glm::radians(2.0F)};
    constexpr float movementEpsilon {1e-5F};
    constexpr glm::vec3 worldUp {0.0F, 1.0F, 0.0F};
    constexpr glm::vec3 lightTarget {0.0F, 0.0F, 0.0F};
    constexpr float shadowFovRadians {glm::radians(60.0F)};
    constexpr float lightTargetEpsilon {1e-4F};
    constexpr glm::vec3 lightFallbackDirection {0.0F, -1.0F, 0.0F};

    struct GlobalUniform final {
        glm::mat4 view {1.0F};
        glm::mat4 projection {1.0F};
        std::array<glm::mat4, maxCascades> lightViewProjection {};
        glm::vec4 lightPositionIntensity {0.0F, 0.0F, 0.0F, 1.0F};
        glm::vec4 cameraPosition {0.0F, 0.0F, 0.0F, 1.0F};
        glm::vec4 shadowParams {0.0025F, 0.05F, 1.0F, 1.0F};
        glm::vec4 cascadeSplits {1.0F, 1.0F, 1.0F, 1.0F};
        glm::vec4 cameraClip {0.1F, 100.0F, 0.0F, 0.0F};
    };

    struct PrimitivePushConstants final {
        glm::mat4 model {1.0F};
        glm::vec4 materialColor {1.0F, 1.0F, 1.0F, 1.0F};
        glm::vec4 materialProperties {32.0F, 0.1F, 0.5F, 0.0F};
    };

    struct ShadowPushConstants final {
        glm::mat4 model {1.0F};
        glm::mat4 lightViewProjection {1.0F};
    };
}

namespace vulkano {

VulkanApplication::VulkanApplication(const AppConfig& config)
    : m_config {config}
    , m_glfwContext {}
    , m_window {Window::create(m_glfwContext, m_config)}
    , m_context {VulkanContextBuilder {}.with_config(m_config).with_window(m_window).build()}
    , m_swapchain {Swapchain::create(m_context, m_window)}
    , m_renderPass {}
    , m_framebuffers {}
    , m_commandAllocator {}
    , m_syncManager {SyncManager::create(m_context, maxFramesInFlight)}
    , m_deviceName {m_context.device_properties().deviceName} {
    m_depthFormat = DepthResources::find_supported_format(m_context);
    const VkExtent2D extent = m_swapchain.extent();
    const std::uint32_t imageCount = static_cast<std::uint32_t>(m_swapchain.image_views().size());

    m_renderPass = RenderPass::create(m_context, m_swapchain.image_format(), m_depthFormat);
    m_depthResources = DepthResources::create(m_context, m_depthFormat, extent, imageCount);
    m_shadowMap = ShadowMap::create(m_context, m_shadowSettings.resolution, m_shadowSettings.cascadeCount);
    m_shadowRenderPass = ShadowRenderPass::create(m_context, m_shadowMap.format());
    create_shadow_framebuffer();
    m_shadowPipeline = ShadowPipeline::create(m_context, m_shadowRenderPass, VK_NULL_HANDLE);
    m_framebuffers = FramebufferCollection::create(
        m_context,
        m_swapchain,
        m_renderPass,
        m_depthResources.image_views());
    m_commandAllocator = CommandAllocator::create(m_context, static_cast<std::uint32_t>(m_framebuffers.size()));
    create_descriptor_resources();
    m_pipeline = GraphicsPipeline::create(m_context, m_renderPass, m_descriptorSetLayout);
    create_render_finished_semaphores();
    m_imagesInFlight.resize(m_swapchain.image_views().size(), VK_NULL_HANDLE);

    initialise_scene();
    init_imgui();
    register_callbacks();
    m_lastFrameTime = std::chrono::steady_clock::now();
}

VulkanApplication::~VulkanApplication() {
    wait_for_device_idle();
    destroy_shadow_framebuffer();
    destroy_descriptor_resources();
    destroy_render_finished_semaphores();
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

auto VulkanApplication::primitive_count() const noexcept -> std::size_t {
    return m_scene.primitives.size();
}

auto VulkanApplication::scene_light_position() const noexcept -> glm::vec3 {
    return m_scene.lightPosition;
}

auto VulkanApplication::scene_light_intensity() const noexcept -> float {
    return m_scene.lightIntensity;
}

void VulkanApplication::register_callbacks() {
    GLFWwindow* windowHandle = m_window.handle();
    if(windowHandle == nullptr) {
        return;
    }
    glfwSetWindowUserPointer(windowHandle, this);
    glfwSetFramebufferSizeCallback(windowHandle, &VulkanApplication::framebuffer_resize_callback);
    glfwSetScrollCallback(windowHandle, &VulkanApplication::scroll_callback);
}

void VulkanApplication::framebuffer_resize_callback(GLFWwindow* window, int, int) {
    auto* app = static_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));
    if(app == nullptr) {
        return;
    }
    app->m_framebufferResized = true;
}

void VulkanApplication::scroll_callback(GLFWwindow* window, double, double yOffset) {
    auto* app = static_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));
    if(app == nullptr) {
        return;
    }
    app->m_scrollDelta += yOffset;
}

void VulkanApplication::draw_frame() {
    const auto now = std::chrono::steady_clock::now();
    const double deltaSeconds = std::chrono::duration<double>(now - m_lastFrameTime).count();
    m_lastFrameTime = now;
    update_timing(deltaSeconds);
    rebuild_dirty_meshes();
    if(m_shadowResourcesDirty) {
        recreate_shadow_resources();
        m_shadowResourcesDirty = false;
    }

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
    ImGuiIO& io = ImGui::GetIO();
    update_camera_input(deltaSeconds, io);
    apply_scroll_input(io);
    update_global_uniforms();

    const VkExtent2D extent = m_swapchain.extent();
    ImGui::SetNextWindowBgAlpha(0.35F);
    ImGui::SetNextWindowSize(ImVec2 {0.0F, 0.0F}, ImGuiCond_Always);
    ImGui::Begin("Runtime Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::Text("FPS: %.2f", m_fps);
    ImGui::Text("Frame Time: %.2f ms", m_frameTimeMs);
    ImGui::Text("Device: %s", m_deviceName.c_str());
    ImGui::Text("Swapchain: %u x %u", extent.width, extent.height);
    ImGui::End();

    ImGui::Begin("Scene Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    if(ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        glm::vec3 lightPosition = m_scene.lightPosition;
        if(ImGui::DragFloat3("Position", glm::value_ptr(lightPosition), 0.1F)) {
            m_scene.lightPosition = lightPosition;
        }

        float lightIntensity = m_scene.lightIntensity;
        if(ImGui::SliderFloat("Intensity", &lightIntensity, 0.0F, 10.0F, "%.2f")) {
            m_scene.lightIntensity = std::max(lightIntensity, 0.0F);
        }
    }

    if(ImGui::CollapsingHeader("Shadows", ImGuiTreeNodeFlags_DefaultOpen)) {
        float depthBiasConstant = m_shadowSettings.depthBiasConstant;
        if(ImGui::SliderFloat("Depth Bias Constant", &depthBiasConstant, 0.0F, 10.0F, "%.3f")) {
            m_shadowSettings.depthBiasConstant = depthBiasConstant;
        }

        float depthBiasSlope = m_shadowSettings.depthBiasSlope;
        if(ImGui::SliderFloat("Depth Bias Slope", &depthBiasSlope, 0.0F, 10.0F, "%.3f")) {
            m_shadowSettings.depthBiasSlope = depthBiasSlope;
        }

        float minBias = m_shadowSettings.minBias;
        if(ImGui::SliderFloat("Receiver Min Bias", &minBias, 0.0F, 0.02F, "%.5f")) {
            m_shadowSettings.minBias = std::clamp(minBias, 0.0F, 0.02F);
        }

        float normalBiasFactor = m_shadowSettings.normalBiasFactor;
        if(ImGui::SliderFloat("Normal Bias Factor", &normalBiasFactor, 0.0F, 0.5F, "%.4f")) {
            m_shadowSettings.normalBiasFactor = std::clamp(normalBiasFactor, 0.0F, 0.5F);
        }

        int pcfRadius = m_shadowSettings.pcfRadius;
        if(ImGui::SliderInt("PCF Radius", &pcfRadius, 0, 4)) {
            m_shadowSettings.pcfRadius = std::clamp(pcfRadius, 0, 4);
        }

        int cascadeCount = static_cast<int>(m_shadowSettings.cascadeCount);
        if(ImGui::SliderInt("Cascade Count", &cascadeCount, 1, static_cast<int>(maxCascades))) {
            cascadeCount = std::clamp(cascadeCount, 1, static_cast<int>(maxCascades));
            if(m_shadowSettings.cascadeCount != static_cast<std::uint32_t>(cascadeCount)) {
                m_shadowSettings.cascadeCount = static_cast<std::uint32_t>(cascadeCount);
                m_shadowResourcesDirty = true;
            }
        }

        float splitLambda = m_shadowSettings.splitLambda;
        if(ImGui::SliderFloat("Split Lambda", &splitLambda, 0.0F, 1.0F, "%.2f")) {
            m_shadowSettings.splitLambda = std::clamp(splitLambda, 0.0F, 1.0F);
        }

        static constexpr std::array<std::uint32_t, 3U> shadowResOptions {1024U, 2048U, 4096U};
        int resolutionIndex = 0;
        for(std::size_t optionIndex {0U}; optionIndex < shadowResOptions.size(); ++optionIndex) {
            if(m_shadowSettings.resolution == shadowResOptions.at(optionIndex)) {
                resolutionIndex = static_cast<int>(optionIndex);
                break;
            }
        }
        const char* resolutionLabels[] {"1024", "2048", "4096"};
        if(ImGui::Combo("Resolution", &resolutionIndex, resolutionLabels, static_cast<int>(shadowResOptions.size()))) {
            const std::uint32_t selectedResolution = shadowResOptions.at(static_cast<std::size_t>(resolutionIndex));
            if(m_shadowSettings.resolution != selectedResolution) {
                m_shadowSettings.resolution = selectedResolution;
                m_shadowResourcesDirty = true;
            }
        }
        ImGui::TextDisabled("Adjust bias to reduce acne; increasing PCF radius softens shadows at extra cost.");

        if(!m_shadowDebugTextures.empty()) {
            if(ImGui::TreeNodeEx("Cascade Maps", ImGuiTreeNodeFlags_DefaultOpen)) {
                const float previewSize {128.0F};
                const std::size_t previewCount = std::min<std::size_t>(
                    m_shadowDebugTextures.size(),
                    static_cast<std::size_t>(m_activeCascadeCount));
                for(std::size_t index {0U}; index < previewCount; ++index) {
                    ImGui::PushID(static_cast<int>(index));
                    ImGui::Image(m_shadowDebugTextures.at(index), ImVec2 {previewSize, previewSize});
                    if(ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("Cascade %zu\nSplit <= %.3f", index, m_cascadeSplits.at(index));
                        ImGui::EndTooltip();
                    }
                    ImGui::PopID();
                    if(index + 1U < previewCount) {
                        ImGui::SameLine();
                    }
                }
                ImGui::TreePop();
            }
        }
    }

    for(std::size_t index {0U}; index < m_scene.primitives.size(); ++index) {
        ScenePrimitive& primitive = m_scene.primitives.at(index);
        if(primitive.primitive == nullptr) {
            continue;
        }

        Primitive& base = *primitive.primitive;
        PrimitiveProperties& properties = base.properties();

        std::string headerLabel = std::string {base.identifier()} + "##Primitive" + std::to_string(index);
        if(ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushID(static_cast<int>(index));

            glm::vec3 position = properties.position;
            if(ImGui::DragFloat3("Position", glm::value_ptr(position), 0.05F)) {
                properties.position = position;
            }

            glm::vec3 rotationDegrees = properties.rotation;
            if(ImGui::DragFloat3("Rotation", glm::value_ptr(rotationDegrees), 0.5F)) {
                properties.rotation = rotationDegrees;
            }

            glm::vec3 scale = properties.scale;
            if(ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.02F, 0.01F, 100.0F, "%.2f")) {
                properties.scale = glm::vec3 {
                    std::max(scale.x, 0.01F),
                    std::max(scale.y, 0.01F),
                    std::max(scale.z, 0.01F)
                };
            }

            glm::vec3 color = properties.baseColor;
            if(ImGui::ColorEdit3("Base Color", glm::value_ptr(color))) {
                properties.baseColor = glm::vec3 {
                    std::clamp(color.x, 0.0F, 1.0F),
                    std::clamp(color.y, 0.0F, 1.0F),
                    std::clamp(color.z, 0.0F, 1.0F)
                };
            }

            float shininess = properties.shininess;
            if(ImGui::SliderFloat("Shininess", &shininess, 1.0F, 256.0F, "%.1f")) {
                properties.shininess = shininess;
            }

            float ambient = properties.ambientStrength;
            if(ImGui::SliderFloat("Ambient", &ambient, 0.0F, 1.0F, "%.2f")) {
                properties.ambientStrength = std::clamp(ambient, 0.0F, 1.0F);
            }

            float specular = properties.specularStrength;
            if(ImGui::SliderFloat("Specular", &specular, 0.0F, 1.0F, "%.2f")) {
                properties.specularStrength = std::clamp(specular, 0.0F, 1.0F);
            }

            if(auto* icosphere = dynamic_cast<IcospherePrimitive*>(primitive.primitive.get())) {
                const IcosphereParameters parameters = icosphere->parameters();
                int subdivisions = static_cast<int>(parameters.subdivisions);
                constexpr int minSubdivisionsInt {0};
                constexpr int maxSubdivisionsInt {5};
                if(ImGui::SliderInt("Subdivisions", &subdivisions, minSubdivisionsInt, maxSubdivisionsInt)) {
                    subdivisions = std::clamp(subdivisions, minSubdivisionsInt, maxSubdivisionsInt);
                    icosphere->set_subdivisions(static_cast<std::uint32_t>(subdivisions));
                }
            }

            ImGui::PopID();
        }
    }

    ImGui::End();

    ImGui::Begin("Camera", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    const glm::vec3 cameraPosition = m_camera.position;
    ImGui::Text("Position: %.2f, %.2f, %.2f", cameraPosition.x, cameraPosition.y, cameraPosition.z);
    const float yawDegrees = glm::degrees(m_camera.yaw);
    const float pitchDegrees = glm::degrees(m_camera.pitch);
    const float fovDegrees = glm::degrees(m_camera.fovY);
    ImGui::Text("Yaw: %.2f deg", yawDegrees);
    ImGui::Text("Pitch: %.2f deg", pitchDegrees);
    ImGui::Text("FOV: %.2f deg", fovDegrees);
    bool lockCamera = m_cameraLock;
    if(ImGui::Checkbox("Lock camera", &lockCamera)) {
        m_cameraLock = lockCamera;
        if(m_cameraLock) {
            if(m_cameraLookActive) {
                m_cameraLookActive = false;
                set_cursor_mode(GLFW_CURSOR_NORMAL);
            }
        }
    }
    ImGui::TextDisabled("Hold RMB to look. WASD to move. Space/E up, Ctrl/Q down. Shift to sprint.");
    ImGui::End();

    ImGui::Render();
    record_command_buffer(imageIndex);

    const VkCommandBuffer commandBuffer = m_commandAllocator.buffers().at(imageIndex);
    const VkPipelineStageFlags waitStages[] {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore* const renderFinishedSemaphore = &m_renderFinishedSemaphores.at(imageIndex);

    VkSubmitInfo submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1U;
    submitInfo.pWaitSemaphores = &frameSync.imageAvailable;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1U;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1U;
    submitInfo.pSignalSemaphores = renderFinishedSemaphore;

    if(vkQueueSubmit(m_context.graphics_queue(), 1U, &submitInfo, frameSync.inFlight) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to submit draw command buffer"};
    }

    VkPresentInfoKHR presentInfo {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1U;
    presentInfo.pWaitSemaphores = renderFinishedSemaphore;
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
    const VkExtent2D extent = m_swapchain.extent();
    const std::uint32_t imageCount = static_cast<std::uint32_t>(m_swapchain.image_views().size());

    m_renderPass = RenderPass::create(m_context, m_swapchain.image_format(), m_depthFormat);
    m_depthResources.recreate(m_context, m_depthFormat, extent, imageCount);
    m_pipeline.recreate(m_context, m_renderPass, m_descriptorSetLayout);
    m_framebuffers.recreate(m_context, m_swapchain, m_renderPass, m_depthResources.image_views());
    const std::uint32_t commandBufferCount = static_cast<std::uint32_t>(m_framebuffers.size());
    m_commandAllocator.recreate(m_context, commandBufferCount);
    m_imagesInFlight.assign(commandBufferCount, VK_NULL_HANDLE);
    create_render_finished_semaphores();
    ImGui_ImplVulkan_SetMinImageCount(static_cast<std::uint32_t>(m_swapchain.image_views().size()));
    update_global_uniforms();
}

void VulkanApplication::create_render_finished_semaphores() {
    destroy_render_finished_semaphores();

    const VkDevice device = m_context.device();
    if(device == VK_NULL_HANDLE) {
        return;
    }

    const std::uint32_t imageCount = static_cast<std::uint32_t>(m_swapchain.image_views().size());
    m_renderFinishedSemaphores.resize(imageCount, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for(std::uint32_t index {0U}; index < imageCount; ++index) {
        VkSemaphore& semaphore = m_renderFinishedSemaphores.at(index);
        if(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create render-finished semaphore"};
        }
        const std::string name = "Swapchain Image " + std::to_string(index) + " Render Finished Semaphore";
        m_context.set_object_name(
            VK_OBJECT_TYPE_SEMAPHORE,
            reinterpret_cast<std::uint64_t>(semaphore),
            name);
    }
}

void VulkanApplication::create_shadow_framebuffer() {
    destroy_shadow_framebuffer();

    if(m_shadowRenderPass.handle() == VK_NULL_HANDLE || m_shadowMap.layer_count() == 0U) {
        return;
    }

    const std::vector<VkImageView>& layerViews = m_shadowMap.layer_views();
    m_shadowFramebuffers.resize(layerViews.size(), VK_NULL_HANDLE);

    for(std::size_t index {0U}; index < layerViews.size(); ++index) {
        const VkImageView attachment = layerViews.at(index);
        VkFramebufferCreateInfo framebufferInfo {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_shadowRenderPass.handle();
        framebufferInfo.attachmentCount = 1U;
        framebufferInfo.pAttachments = &attachment;
        framebufferInfo.width = m_shadowMap.resolution();
        framebufferInfo.height = m_shadowMap.resolution();
        framebufferInfo.layers = 1U;

        if(vkCreateFramebuffer(m_context.device(), &framebufferInfo, nullptr, &m_shadowFramebuffers.at(index)) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create shadow framebuffer"};
        }

        const std::string name = "Shadow Framebuffer Layer " + std::to_string(index);
        m_context.set_object_name(
            VK_OBJECT_TYPE_FRAMEBUFFER,
            reinterpret_cast<std::uint64_t>(m_shadowFramebuffers.at(index)),
            name);
    }
}

void VulkanApplication::destroy_shadow_framebuffer() noexcept {
    destroy_shadow_debug_textures();
    for(VkFramebuffer framebuffer : m_shadowFramebuffers) {
        if(framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_context.device(), framebuffer, nullptr);
        }
    }
    m_shadowFramebuffers.clear();
}

void VulkanApplication::destroy_shadow_debug_textures() noexcept {
    if(m_shadowDebugTextures.empty()) {
        return;
    }
    for(ImTextureID texture : m_shadowDebugTextures) {
        if(texture != nullptr) {
            ImGui_ImplVulkan_RemoveTexture(static_cast<VkDescriptorSet>(texture));
        }
    }
    m_shadowDebugTextures.clear();
}

void VulkanApplication::update_shadow_debug_textures() {
    if(ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    destroy_shadow_debug_textures();
    const auto& layerViews = m_shadowMap.layer_views();
    m_shadowDebugTextures.reserve(layerViews.size());
    for(VkImageView view : layerViews) {
        VkDescriptorSet descriptor = ImGui_ImplVulkan_AddTexture(
            m_shadowMap.sampler(),
            view,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        m_shadowDebugTextures.push_back(descriptor);
    }
}

void VulkanApplication::recreate_shadow_resources() {
    wait_for_device_idle();
    destroy_shadow_debug_textures();
    m_shadowMap = ShadowMap::create(m_context, m_shadowSettings.resolution, m_shadowSettings.cascadeCount);
    create_shadow_framebuffer();
    if(m_descriptorSet != VK_NULL_HANDLE) {
        VkDescriptorImageInfo imageInfo {};
        imageInfo.sampler = m_shadowMap.sampler();
        imageInfo.imageView = m_shadowMap.layered_view();
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_descriptorSet;
        write.dstBinding = 1U;
        write.dstArrayElement = 0U;
        write.descriptorCount = 1U;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_context.device(), 1U, &write, 0U, nullptr);
    }
    update_global_uniforms();
    update_shadow_debug_textures();
}

void VulkanApplication::destroy_render_finished_semaphores() noexcept {
    const VkDevice device = m_context.device();
    if(device == VK_NULL_HANDLE) {
        m_renderFinishedSemaphores.clear();
        return;
    }
    for(VkSemaphore& semaphore : m_renderFinishedSemaphores) {
        if(semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, semaphore, nullptr);
            semaphore = VK_NULL_HANDLE;
        }
    }
    m_renderFinishedSemaphores.clear();
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
    upload_imgui_fonts();
    update_shadow_debug_textures();
}

void VulkanApplication::destroy_imgui() {
    destroy_shadow_debug_textures();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if(m_imguiDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_context.device(), m_imguiDescriptorPool, nullptr);
        m_imguiDescriptorPool = VK_NULL_HANDLE;
    }
}

void VulkanApplication::upload_imgui_fonts() {
    if(!ImGui_ImplVulkan_CreateFontsTexture()) {
        throw std::runtime_error {"Failed to upload ImGui fonts"};
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

    std::array<VkClearValue, 2U> clearValues {};
    clearValues[0].color = {{clearColour[0], clearColour[1], clearColour[2], clearColour[3]}};
    clearValues[1].depthStencil = {1.0F, 0U};

    const std::uint32_t availableCascadeCount = std::min<std::uint32_t>(
        m_activeCascadeCount,
        static_cast<std::uint32_t>(m_shadowFramebuffers.size()));

    if(!m_shadowFramebuffers.empty() && availableCascadeCount > 0U) {
        m_context.begin_debug_label(commandBuffer, "Shadow Map Pass");

        const float depthBiasConstant = std::max(m_shadowSettings.depthBiasConstant, 0.0F);
        const float depthBiasSlope = std::max(m_shadowSettings.depthBiasSlope, 0.0F);

        for(std::uint32_t cascadeIndex {0U}; cascadeIndex < availableCascadeCount; ++cascadeIndex) {
            const VkFramebuffer framebuffer = m_shadowFramebuffers.at(cascadeIndex);
            if(framebuffer == VK_NULL_HANDLE) {
                continue;
            }

            VkClearValue depthClear {};
            depthClear.depthStencil = {1.0F, 0U};

            VkRenderPassBeginInfo shadowPassInfo {};
            shadowPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            shadowPassInfo.renderPass = m_shadowRenderPass.handle();
            shadowPassInfo.framebuffer = framebuffer;
            shadowPassInfo.renderArea.offset = {0, 0};
            shadowPassInfo.renderArea.extent = {
                m_shadowMap.resolution(),
                m_shadowMap.resolution()
            };
            shadowPassInfo.clearValueCount = 1U;
            shadowPassInfo.pClearValues = &depthClear;

            vkCmdBeginRenderPass(commandBuffer, &shadowPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport shadowViewport {};
            shadowViewport.x = 0.0F;
            shadowViewport.y = 0.0F;
            shadowViewport.width = static_cast<float>(m_shadowMap.resolution());
            shadowViewport.height = static_cast<float>(m_shadowMap.resolution());
            shadowViewport.minDepth = 0.0F;
            shadowViewport.maxDepth = 1.0F;
            vkCmdSetViewport(commandBuffer, 0U, 1U, &shadowViewport);

            VkRect2D shadowScissor {};
            shadowScissor.offset = {0, 0};
            shadowScissor.extent = {
                m_shadowMap.resolution(),
                m_shadowMap.resolution()
            };
            vkCmdSetScissor(commandBuffer, 0U, 1U, &shadowScissor);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.handle());
            vkCmdSetDepthBias(commandBuffer, depthBiasConstant, 0.0F, depthBiasSlope);

            for(const ScenePrimitive& primitive : m_scene.primitives) {
                if(primitive.primitive == nullptr || !primitive.gpu.has_geometry()) {
                    continue;
                }

                const VkBuffer vertexBuffers[] {primitive.gpu.vertex_buffer()};
                const VkDeviceSize offsets[] {0U};
                vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(commandBuffer, primitive.gpu.index_buffer(), 0U, VK_INDEX_TYPE_UINT32);

                const PrimitiveProperties& properties = primitive.primitive->properties();
                ShadowPushConstants shadowPush {};
                shadowPush.model = compute_model_matrix(properties);
                shadowPush.lightViewProjection = m_cascadeMatrices.at(cascadeIndex);

                vkCmdPushConstants(
                    commandBuffer,
                    m_shadowPipeline.layout(),
                    VK_SHADER_STAGE_VERTEX_BIT,
                    0U,
                    static_cast<std::uint32_t>(sizeof(ShadowPushConstants)),
                    &shadowPush);

                vkCmdDrawIndexed(commandBuffer, primitive.gpu.index_count(), 1U, 0U, 0, 0);
            }

            vkCmdEndRenderPass(commandBuffer);
        }

        m_context.end_debug_label(commandBuffer);

        VkImageMemoryBarrier shadowBarrier {};
        shadowBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        shadowBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        shadowBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        shadowBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        shadowBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        shadowBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        shadowBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        shadowBarrier.image = m_shadowMap.image();
        shadowBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        shadowBarrier.subresourceRange.baseMipLevel = 0U;
        shadowBarrier.subresourceRange.levelCount = 1U;
        shadowBarrier.subresourceRange.baseArrayLayer = 0U;
        shadowBarrier.subresourceRange.layerCount = availableCascadeCount;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0U,
            0U,
            nullptr,
            0U,
            nullptr,
            1U,
            &shadowBarrier);
    }

    VkRenderPassBeginInfo renderPassInfo {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass.handle();
    renderPassInfo.framebuffer = m_framebuffers.handles().at(imageIndex);
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapchain.extent();
    renderPassInfo.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    const VkExtent2D extent = m_swapchain.extent();

    VkViewport viewport {};
    viewport.x = 0.0F;
    viewport.y = static_cast<float>(extent.height);
    viewport.width = static_cast<float>(extent.width);
    viewport.height = -static_cast<float>(extent.height);
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;
    vkCmdSetViewport(commandBuffer, 0U, 1U, &viewport);

    VkRect2D scissor {};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(commandBuffer, 0U, 1U, &scissor);

    m_context.begin_debug_label(commandBuffer, "Draw Primitives");
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.handle());

    if(m_descriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipeline.layout(),
            0U,
            1U,
            &m_descriptorSet,
            0U,
            nullptr);
    }

    for(const ScenePrimitive& primitive : m_scene.primitives) {
        if(primitive.primitive == nullptr || !primitive.gpu.has_geometry()) {
            continue;
        }

        const VkBuffer vertexBuffers[] {primitive.gpu.vertex_buffer()};
        const VkDeviceSize offsets[] {0U};
        vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, primitive.gpu.index_buffer(), 0U, VK_INDEX_TYPE_UINT32);

        const PrimitiveProperties& properties = primitive.primitive->properties();
        PrimitivePushConstants pushConstants {};
        pushConstants.model = compute_model_matrix(properties);
        pushConstants.materialColor = glm::vec4 {properties.baseColor, 1.0F};
        pushConstants.materialProperties = glm::vec4 {
            properties.shininess,
            properties.ambientStrength,
            properties.specularStrength,
            0.0F
        };

        vkCmdPushConstants(
            commandBuffer,
            m_pipeline.layout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0U,
            static_cast<std::uint32_t>(sizeof(PrimitivePushConstants)),
            &pushConstants);

        vkCmdDrawIndexed(commandBuffer, primitive.gpu.index_count(), 1U, 0U, 0, 0);
    }

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
        m_deltaSeconds = 0.0;
        return;
    }
    m_deltaSeconds = deltaSeconds;
    const double frameTimeMs = deltaSeconds * 1000.0;
    if(m_frameTimeMs <= 0.0) {
        m_frameTimeMs = frameTimeMs;
    } else {
        m_frameTimeMs = (timingSmoothingFactor * frameTimeMs) + ((1.0 - timingSmoothingFactor) * m_frameTimeMs);
    }
    m_fps = (m_frameTimeMs > 0.0) ? 1000.0 / m_frameTimeMs : 0.0;
}

void VulkanApplication::update_camera_input(double deltaSeconds, const ImGuiIO& io) {
    GLFWwindow* windowHandle = m_window.handle();
    if(windowHandle == nullptr) {
        m_cameraLookActive = false;
        return;
    }

    const int rightMouseState = glfwGetMouseButton(windowHandle, GLFW_MOUSE_BUTTON_RIGHT);
    const bool rmbPressed = rightMouseState == GLFW_PRESS;
    const bool wantsMouse = io.WantCaptureMouse;
    const bool shouldLook = rmbPressed && !wantsMouse && !m_cameraLock;

    if(shouldLook && !m_cameraLookActive) {
        m_cameraLookActive = true;
        m_firstMouse = true;
        set_cursor_mode(GLFW_CURSOR_DISABLED);
    }

    if((!shouldLook || m_cameraLock) && m_cameraLookActive) {
        m_cameraLookActive = false;
        set_cursor_mode(GLFW_CURSOR_NORMAL);
    }

    if(!m_cameraLookActive) {
        m_firstMouse = true;
    }

    if(m_cameraLookActive) {
        double cursorX {0.0};
        double cursorY {0.0};
        glfwGetCursorPos(windowHandle, &cursorX, &cursorY);
        const glm::dvec2 current {cursorX, cursorY};
        if(m_firstMouse) {
            m_lastCursorPosition = current;
            m_firstMouse = false;
        }
        const glm::dvec2 delta {
            current.x - m_lastCursorPosition.x,
            m_lastCursorPosition.y - current.y
        };
        m_lastCursorPosition = current;

        const float yawDelta = static_cast<float>(delta.x) * mouseSensitivity;
        const float pitchDelta = static_cast<float>(delta.y) * mouseSensitivity;
        m_camera.yaw += yawDelta;
        m_camera.pitch = clamp_pitch(m_camera.pitch + pitchDelta, -pitchLimitRadians, pitchLimitRadians);
    }

    const bool wantsKeyboard = io.WantCaptureKeyboard;
    const bool allowMovement = !m_cameraLock && (m_cameraLookActive || !wantsKeyboard);
    if(!allowMovement) {
        return;
    }

    const float deltaSecondsF = static_cast<float>(std::max(deltaSeconds, 0.0));
    if(deltaSecondsF <= 0.0F) {
        return;
    }

    const glm::vec3 forward = camera_forward();
    glm::vec3 horizontalForward {forward.x, 0.0F, forward.z};
    const float forwardLength = glm::length(horizontalForward);
    if(forwardLength > movementEpsilon) {
        horizontalForward /= forwardLength;
    } else {
        horizontalForward = glm::vec3 {0.0F, 0.0F, -1.0F};
    }

    glm::vec3 right = camera_right(forward);
    right.y = 0.0F;
    const float rightLength = glm::length(right);
    if(rightLength > movementEpsilon) {
        right /= rightLength;
    } else {
        right = glm::vec3 {1.0F, 0.0F, 0.0F};
    }

    glm::vec3 movement {0.0F, 0.0F, 0.0F};
    if(glfwGetKey(windowHandle, GLFW_KEY_W) == GLFW_PRESS) {
        movement += horizontalForward;
    }
    if(glfwGetKey(windowHandle, GLFW_KEY_S) == GLFW_PRESS) {
        movement -= horizontalForward;
    }
    if(glfwGetKey(windowHandle, GLFW_KEY_A) == GLFW_PRESS) {
        movement -= right;
    }
    if(glfwGetKey(windowHandle, GLFW_KEY_D) == GLFW_PRESS) {
        movement += right;
    }
    if(glfwGetKey(windowHandle, GLFW_KEY_SPACE) == GLFW_PRESS) {
        movement += worldUp;
    }
    if(glfwGetKey(windowHandle, GLFW_KEY_E) == GLFW_PRESS) {
        movement += worldUp;
    }
    if(glfwGetKey(windowHandle, GLFW_KEY_Q) == GLFW_PRESS) {
        movement -= worldUp;
    }
    if(glfwGetKey(windowHandle, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS
        || glfwGetKey(windowHandle, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) {
        movement -= worldUp;
    }

    const float movementLength = glm::length(movement);
    if(movementLength <= movementEpsilon) {
        return;
    }
    movement /= movementLength;

    const bool sprinting = (glfwGetKey(windowHandle, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        || (glfwGetKey(windowHandle, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
    const float speed = cameraBaseSpeed * (sprinting ? cameraSprintMultiplier : 1.0F);
    const float distance = speed * deltaSecondsF;
    m_camera.position += movement * distance;
}

void VulkanApplication::apply_scroll_input(const ImGuiIO& io) {
    if(m_scrollDelta == 0.0) {
        return;
    }
    if(io.WantCaptureMouse || m_cameraLock) {
        m_scrollDelta = 0.0;
        return;
    }

    const float scrollAmount = static_cast<float>(m_scrollDelta) * fovScrollStep;
    m_camera.fovY = adjust_fov(m_camera.fovY, scrollAmount, minFovRadians, maxFovRadians);
    m_scrollDelta = 0.0;
}

void VulkanApplication::set_cursor_mode(int mode) noexcept {
    GLFWwindow* windowHandle = m_window.handle();
    if(windowHandle == nullptr) {
        return;
    }
    glfwSetInputMode(windowHandle, GLFW_CURSOR, mode);
}

auto VulkanApplication::camera_forward() const noexcept -> glm::vec3 {
    return compute_forward(m_camera.yaw, m_camera.pitch);
}

auto VulkanApplication::camera_right(const glm::vec3& forward) const noexcept -> glm::vec3 {
    return compute_right(forward, worldUp);
}

auto VulkanApplication::camera_up(const glm::vec3& forward, const glm::vec3& right) const noexcept -> glm::vec3 {
    return compute_up(forward, right);
}

void VulkanApplication::initialise_scene() {
    constexpr PlaneParameters planeParameters {
        .width = 10.0F,
        .depth = 10.0F,
        .uvTiling = glm::vec2 {4.0F, 4.0F}
    };

    PrimitiveProperties planeProperties {};
    planeProperties.baseColor = glm::vec3 {0.6F, 0.6F, 0.6F};

    PrimitiveProperties cubeProperties {};
    cubeProperties.position = glm::vec3 {-1.0F, 0.5F, 0.0F};
    cubeProperties.scale = glm::vec3 {0.5F, 0.5F, 0.5F};
    cubeProperties.baseColor = glm::vec3 {0.9F, 0.4F, 0.1F};

    PrimitiveProperties sphereProperties {};
    sphereProperties.position = glm::vec3 {1.0F, 0.5F, 0.0F};
    sphereProperties.scale = glm::vec3 {0.5F, 0.5F, 0.5F};
    sphereProperties.baseColor = glm::vec3 {0.3F, 0.6F, 0.9F};

    IcosphereParameters sphereParameters {};
    sphereParameters.subdivisions = 2U;

    std::vector<std::unique_ptr<Primitive>> primitives {};
    primitives.push_back(std::make_unique<PlanePrimitive>(planeParameters, planeProperties));
    primitives.push_back(std::make_unique<CubePrimitive>(cubeProperties));
    primitives.push_back(std::make_unique<IcospherePrimitive>(sphereParameters, sphereProperties));

    m_scene.primitives.clear();
    m_scene.primitives.reserve(primitives.size());

    for(auto& primitive : primitives) {
        ScenePrimitive scenePrimitive {};
        scenePrimitive.primitive = std::move(primitive);
        if(scenePrimitive.primitive != nullptr) {
            const auto& mesh = scenePrimitive.primitive->mesh();
            const std::string name = std::string {scenePrimitive.primitive->identifier()} + " Primitive";
            scenePrimitive.gpu.upload(m_context, mesh, name);
            scenePrimitive.primitive->clear_mesh_dirty();
        }
        m_scene.primitives.push_back(std::move(scenePrimitive));
    }
}

void VulkanApplication::rebuild_dirty_meshes() {
    for(ScenePrimitive& primitive : m_scene.primitives) {
        if(primitive.primitive == nullptr) {
            continue;
        }
        if(!primitive.primitive->needs_rebuild()) {
            continue;
        }
        primitive.primitive->rebuild_mesh();
        const auto& mesh = primitive.primitive->mesh();
        const std::string name = std::string {primitive.primitive->identifier()} + " Primitive";
        primitive.gpu.upload(m_context, mesh, name);
        primitive.primitive->clear_mesh_dirty();
    }
}

void VulkanApplication::create_descriptor_resources() {
    destroy_descriptor_resources();

    VkDescriptorSetLayoutBinding uniformBinding {};
    uniformBinding.binding = 0U;
    uniformBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformBinding.descriptorCount = 1U;
    uniformBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding shadowBinding {};
    shadowBinding.binding = 1U;
    shadowBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    shadowBinding.descriptorCount = 1U;
    shadowBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2U> bindings {uniformBinding, shadowBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if(vkCreateDescriptorSetLayout(m_context.device(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create descriptor set layout"};
    }
    m_context.set_object_name(
        VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
        reinterpret_cast<std::uint64_t>(m_descriptorSetLayout),
        "Global Descriptor Set Layout");

    std::array<VkDescriptorPoolSize, 2U> poolSizes {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1U;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1U;

    VkDescriptorPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1U;

    if(vkCreateDescriptorPool(m_context.device(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create descriptor pool"};
    }
    m_context.set_object_name(
        VK_OBJECT_TYPE_DESCRIPTOR_POOL,
        reinterpret_cast<std::uint64_t>(m_descriptorPool),
        "Global Descriptor Pool");

    VkDescriptorSetAllocateInfo allocateInfo {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = m_descriptorPool;
    allocateInfo.descriptorSetCount = 1U;
    allocateInfo.pSetLayouts = &m_descriptorSetLayout;

    if(vkAllocateDescriptorSets(m_context.device(), &allocateInfo, &m_descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to allocate descriptor set"};
    }

    m_globalUniformBuffer = Buffer::create_uniform_buffer(m_context, sizeof(GlobalUniform));
    m_context.set_object_name(
        VK_OBJECT_TYPE_BUFFER,
        reinterpret_cast<std::uint64_t>(m_globalUniformBuffer.handle()),
        "Global Uniform Buffer");

    VkDescriptorBufferInfo bufferInfo {};
    bufferInfo.buffer = m_globalUniformBuffer.handle();
    bufferInfo.offset = 0U;
    bufferInfo.range = sizeof(GlobalUniform);

    VkDescriptorImageInfo imageInfo {};
    imageInfo.sampler = m_shadowMap.sampler();
    imageInfo.imageView = m_shadowMap.layered_view();
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    std::array<VkWriteDescriptorSet, 2U> writes {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_descriptorSet;
    writes[0].dstBinding = 0U;
    writes[0].dstArrayElement = 0U;
    writes[0].descriptorCount = 1U;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &bufferInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_descriptorSet;
    writes[1].dstBinding = 1U;
    writes[1].dstArrayElement = 0U;
    writes[1].descriptorCount = 1U;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_context.device(), static_cast<std::uint32_t>(writes.size()), writes.data(), 0U, nullptr);

    update_global_uniforms();
}

void VulkanApplication::destroy_descriptor_resources() noexcept {
    if(m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_context.device(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if(m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_context.device(), m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
    m_descriptorSet = VK_NULL_HANDLE;
    m_globalUniformBuffer = Buffer {};
}

void VulkanApplication::update_global_uniforms() {
    if(m_globalUniformBuffer.handle() == VK_NULL_HANDLE) {
        return;
    }

    GlobalUniform uniforms {};
    const glm::vec3 cameraPos = camera_position();
    uniforms.view = compute_view_matrix(cameraPos, m_camera.yaw, m_camera.pitch, worldUp);

    const VkExtent2D extent = m_swapchain.extent();
    float aspect = 1.0F;
    if(extent.height > 0U) {
        aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
        uniforms.projection = glm::perspective(m_camera.fovY, aspect, m_camera.nearPlane, m_camera.farPlane);
    }

    const std::uint32_t requestedCascadeCount = std::max(1U, std::min<std::uint32_t>(m_shadowSettings.cascadeCount, static_cast<std::uint32_t>(maxCascades)));
    m_shadowSettings.cascadeCount = requestedCascadeCount;
    m_activeCascadeCount = std::min(requestedCascadeCount, m_shadowMap.layer_count());

    const auto splits = compute_cascade_splits(m_camera.nearPlane, m_camera.farPlane, m_activeCascadeCount, m_shadowSettings.splitLambda);
    m_cascadeSplits.fill(1.0F);
    const std::uint32_t cascadeCount = std::max(m_activeCascadeCount, 1U);

    const float clipRange = m_camera.farPlane - m_camera.nearPlane;
    for(std::uint32_t index {0U}; index < cascadeCount; ++index) {
        m_cascadeSplits.at(index) = splits.at(index);
    }

    float previousSplit {m_camera.nearPlane};
    for(std::uint32_t index {0U}; index < cascadeCount; ++index) {
        const float splitNormalized = splits.at(index);
        const float splitDistance = m_camera.nearPlane + clipRange * splitNormalized;

        const glm::mat4 lightMatrix = compute_light_view_projection(
            m_scene.lightPosition,
            lightTarget,
            worldUp,
            shadowFovRadians,
            previousSplit,
            splitDistance,
            lightFallbackDirection,
            lightTargetEpsilon);

        m_cascadeMatrices.at(index) = lightMatrix;
        uniforms.lightViewProjection.at(index) = lightMatrix;
        previousSplit = splitDistance;
    }

    for(std::uint32_t index {cascadeCount}; index < maxCascades; ++index) {
        m_cascadeMatrices.at(index) = glm::mat4 {1.0F};
        m_cascadeSplits.at(index) = 1.0F;
        uniforms.lightViewProjection.at(index) = glm::mat4 {1.0F};
    }

    uniforms.lightPositionIntensity = glm::vec4 {m_scene.lightPosition, m_scene.lightIntensity};
    uniforms.cameraPosition = glm::vec4 {cameraPos, 1.0F};

    const float minBias = std::clamp(m_shadowSettings.minBias, 0.0F, 0.02F);
    const float normalBias = std::clamp(m_shadowSettings.normalBiasFactor, 0.0F, 0.5F);
    const int pcfRadius = std::clamp(m_shadowSettings.pcfRadius, 0, 4);
    m_shadowSettings.minBias = minBias;
    m_shadowSettings.normalBiasFactor = normalBias;
    m_shadowSettings.pcfRadius = pcfRadius;
    uniforms.shadowParams = glm::vec4 {minBias, normalBias, static_cast<float>(pcfRadius), static_cast<float>(cascadeCount)};
    uniforms.cascadeSplits = glm::vec4 {
        m_cascadeSplits.at(0U),
        m_cascadeSplits.at(1U),
        m_cascadeSplits.at(2U),
        m_cascadeSplits.at(3U)
    };
    uniforms.cameraClip = glm::vec4 {m_camera.nearPlane, m_camera.farPlane, clipRange, aspect};

    m_globalUniformBuffer.write(m_context, &uniforms, sizeof(GlobalUniform));
}

auto VulkanApplication::camera_position() const noexcept -> glm::vec3 {
    return m_camera.position;
}

auto VulkanApplication::compute_model_matrix(const PrimitiveProperties& properties) const noexcept -> glm::mat4 {
    glm::mat4 model {1.0F};
    model = glm::translate(model, properties.position);
    model = glm::rotate(model, glm::radians(properties.rotation.x), glm::vec3 {1.0F, 0.0F, 0.0F});
    model = glm::rotate(model, glm::radians(properties.rotation.y), glm::vec3 {0.0F, 1.0F, 0.0F});
    model = glm::rotate(model, glm::radians(properties.rotation.z), glm::vec3 {0.0F, 0.0F, 1.0F});
    model = glm::scale(model, properties.scale);
    return model;
}

} // namespace vulkano
