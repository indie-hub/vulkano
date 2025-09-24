#include <vulkano/application.hpp>
#include <vulkano/camera_math.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <cstdint>
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
    constexpr std::array<float, 4U> clearColour {0.0F, 0.0F, 0.0F, 1.0F};
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
    constexpr float fallbackLightIntensity {1.0F};

    using ShadowUniform = vulkano::CascadedShadowUniform;

    struct alignas(16) GlobalUniform final {
        glm::mat4 view {1.0F};
        glm::mat4 projection {1.0F};
        glm::vec4 lightDirectionIntensity {0.0F, -1.0F, 0.0F, fallbackLightIntensity};
        glm::vec4 cameraPosition {0.0F, 0.0F, 0.0F, 1.0F};
        ShadowUniform shadow {};
    };

    struct alignas(16) PrimitivePushConstants final {
        glm::mat4 model {1.0F};
        glm::vec4 materialColor {1.0F, 1.0F, 1.0F, 1.0F};
        glm::vec4 materialProperties {32.0F, 0.1F, 0.5F, 0.0F};
        glm::vec4 materialFlags {1.0F, 1.0F, 1.0F, 0.0F};
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
    m_framebuffers = FramebufferCollection::create(
        m_context,
        m_swapchain,
        m_renderPass,
        m_depthResources.image_views());
    m_commandAllocator = CommandAllocator::create(m_context, static_cast<std::uint32_t>(m_framebuffers.size()));

    create_descriptor_resources();
    create_shadow_resources();
    create_texture_resources();

    const std::array<VkDescriptorSetLayout, 2U> descriptorLayouts {
        m_descriptorSetLayout,
        m_materialDescriptorSetLayout
    };

    m_pipeline = GraphicsPipeline::create(m_context, m_renderPass, descriptorLayouts);
    m_shadowPipeline = ShadowPipeline::create(m_context, m_shadowRenderPass, descriptorLayouts);
    create_render_finished_semaphores();
    m_imagesInFlight.resize(m_swapchain.image_views().size(), VK_NULL_HANDLE);

    initialise_scene();
    allocate_material_descriptor_pool(static_cast<std::uint32_t>(m_scene.primitives.size()));
    init_imgui();
    update_shadow_debug_textures();
    register_callbacks();
    m_lastFrameTime = std::chrono::steady_clock::now();
}

VulkanApplication::~VulkanApplication() {
    wait_for_device_idle();
    destroy_shadow_resources();
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

auto VulkanApplication::scene_light_direction() const noexcept -> glm::vec3 {
    return m_scene.lightDirection;
}

auto VulkanApplication::scene_light_position() const noexcept -> glm::vec3 {
    return m_scene.lightDirection;
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
    if(m_shadow.settings.showShadowAtlas && m_shadow.debugDescriptorsDirty) {
        update_shadow_debug_textures();
    }
    ImGuiIO& io = ImGui::GetIO();
    update_camera_input(deltaSeconds, io);
    apply_scroll_input(io);

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
        glm::vec3 lightVector = m_scene.lightDirection;
        if(ImGui::DragFloat3("Direction", glm::value_ptr(lightVector), 0.05F, -10.0F, 10.0F, "%.2f")) {
            const float vectorLength = glm::length(lightVector);
            if(vectorLength > movementEpsilon) {
                m_scene.lightDirection = lightVector / vectorLength;
            }
        }

        float lightIntensity = m_scene.lightIntensity;
        if(ImGui::SliderFloat("Intensity", &lightIntensity, 0.0F, 10.0F, "%.2f")) {
            m_scene.lightIntensity = std::max(lightIntensity, 0.0F);
        }
    }

    if(ImGui::CollapsingHeader("Cascaded Shadows", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool enabled = m_shadow.settings.enabled;
        if(ImGui::Checkbox("Enabled", &enabled)) {
            m_shadow.settings.enabled = enabled;
        }

        int cascadeCount = static_cast<int>(m_shadow.settings.cascadeCount);
        cascadeCount = std::clamp(cascadeCount, 2, static_cast<int>(maxShadowCascades));
        if(ImGui::SliderInt("Cascade Count", &cascadeCount, 2, static_cast<int>(maxShadowCascades))) {
            m_shadow.settings.cascadeCount = static_cast<std::uint32_t>(cascadeCount);
            m_shadow.resourcesDirty = true;
        }

        const std::array<std::uint32_t, 3U> resolutionOptions {1024U, 2048U, 4096U};
        int resolutionIndex = 0;
        for(std::size_t idx {0U}; idx < resolutionOptions.size(); ++idx) {
            if(m_shadow.settings.resolution == resolutionOptions.at(idx)) {
                resolutionIndex = static_cast<int>(idx);
                break;
            }
        }
        const char* resolutionLabels[] {"1024", "2048", "4096"};
        if(ImGui::Combo("Shadow Map Size", &resolutionIndex, resolutionLabels, static_cast<int>(resolutionOptions.size()))) {
            m_shadow.settings.resolution = resolutionOptions.at(static_cast<std::size_t>(resolutionIndex));
            m_shadow.resourcesDirty = true;
        }

        ImGui::SliderFloat("Split Lambda", &m_shadow.settings.splitLambda, 0.0F, 1.0F, "%.2f");
        ImGui::SliderFloat("Bias (Constant)", &m_shadow.settings.biasConstant, 0.0F, 0.05F, "%.4f");
        ImGui::SliderFloat("Bias (Slope)", &m_shadow.settings.biasSlope, 0.0F, 5.0F, "%.2f");
        ImGui::SliderFloat("Normal Offset", &m_shadow.settings.normalBias, 0.0F, 0.05F, "%.4f");
        ImGui::SliderFloat("PCF Radius", &m_shadow.settings.pcfRadius, 0.0F, 3.0F, "%.1f");
        ImGui::SliderFloat("Shadow Strength", &m_shadow.settings.shadowStrength, 0.0F, 1.0F, "%.2f");

        bool stabilize = m_shadow.settings.stabilize;
        if(ImGui::Checkbox("Stabilize (Texel Snap)", &stabilize)) {
            m_shadow.settings.stabilize = stabilize;
        }

        bool visualizeCascades = m_shadow.settings.visualizeCascades;
        if(ImGui::Checkbox("Visualize Cascades", &visualizeCascades)) {
            m_shadow.settings.visualizeCascades = visualizeCascades;
        }

        bool showAtlas = m_shadow.settings.showShadowAtlas;
        if(ImGui::Checkbox("Show Shadow Map", &showAtlas)) {
            m_shadow.settings.showShadowAtlas = showAtlas;
            if(showAtlas) {
                m_shadow.debugDescriptorsDirty = true;
            } else {
                clear_shadow_debug_textures();
                m_shadow.debugDescriptorsDirty = false;
            }
        }

        if(m_shadow.settings.showShadowAtlas) {
            if(!m_shadow.debugAtlasDescriptors.empty()) {
                ImGui::Separator();
                for(std::size_t cascadeIdx {0U}; cascadeIdx < m_shadow.debugAtlasDescriptors.size(); ++cascadeIdx) {
                    const VkDescriptorSet descriptor = m_shadow.debugAtlasDescriptors.at(cascadeIdx);
                    if(descriptor == VK_NULL_HANDLE) {
                        continue;
                    }
                    ImGui::Text("Cascade %zu", cascadeIdx);
                    const ImTextureID textureId = reinterpret_cast<ImTextureID>(descriptor);
                    ImGui::Image(textureId, ImVec2 {128.0F, 128.0F});
                }
            } else {
                ImGui::Separator();
                ImGui::TextDisabled("Shadow atlas not available yet.");
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

            bool useAlbedo = properties.useAlbedoMap;
            if(ImGui::Checkbox("Use Albedo Map", &useAlbedo)) {
                properties.useAlbedoMap = useAlbedo;
            }

            bool useNormal = properties.useNormalMap;
            if(ImGui::Checkbox("Use Normal Map", &useNormal)) {
                properties.useNormalMap = useNormal;
            }

            float normalStrength = properties.normalStrength;
            if(ImGui::SliderFloat("Normal Strength", &normalStrength, 0.0F, 2.0F, "%.2f")) {
                properties.normalStrength = std::clamp(normalStrength, 0.0F, 2.0F);
            }

            if(auto* plane = dynamic_cast<PlanePrimitive*>(primitive.primitive.get())) {
                PlaneParameters parameters = plane->parameters();
                glm::vec2 uvTiling = parameters.uvTiling;
                if(ImGui::DragFloat2("UV Tiling", glm::value_ptr(uvTiling), 0.1F, 0.1F, 64.0F, "%.2f")) {
                    uvTiling.x = std::max(uvTiling.x, 0.1F);
                    uvTiling.y = std::max(uvTiling.y, 0.1F);
                    parameters.uvTiling = uvTiling;
                    plane->set_parameters(parameters);
                    properties.uvScale = uvTiling;
                }
            }

            const char* albedoLabel = primitive.material.albedoUsesFallback ? "Fallback" : "Custom";
            const VkExtent2D albedoExtent = primitive.material.albedoExtent;
            ImGui::Text(
                "Albedo Map: %s (%u x %u)",
                albedoLabel,
                albedoExtent.width,
                albedoExtent.height);

            const char* normalLabel = primitive.material.normalUsesFallback ? "Fallback" : "Custom";
            const VkExtent2D normalExtent = primitive.material.normalExtent;
            ImGui::Text(
                "Normal Map: %s (%u x %u)",
                normalLabel,
                normalExtent.width,
                normalExtent.height);

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
    ImGui::TextDisabled("Hold RMB to look. WASD to move. Shift to sprint.");
    ImGui::End();

    ImGui::Begin("Texture Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    const bool anisotropyEnabled = m_textureSamplers.anisotropy_enabled();
    ImGui::Text(
        "Anisotropy: %s",
        anisotropyEnabled ? "Enabled" : "Disabled");
    ImGui::Text("Max Anisotropy: %.1f", m_textureSamplers.max_anisotropy());
    ImGui::Text(
        "Fallback Albedo: %u x %u",
        m_fallbackAlbedoExtent.width,
        m_fallbackAlbedoExtent.height);
    ImGui::Text(
        "Fallback Normal: %u x %u",
        m_fallbackNormalExtent.width,
        m_fallbackNormalExtent.height);

    ImGui::Separator();
    for(std::size_t index {0U}; index < m_scene.primitives.size(); ++index) {
        const ScenePrimitive& primitive = m_scene.primitives.at(index);
        const std::string identifier = (primitive.primitive != nullptr)
            ? std::string {primitive.primitive->identifier()}
            : std::string {"Primitive"};
        ImGui::Text(
            "%s Albedo: %s (%u x %u)",
            identifier.c_str(),
            primitive.material.albedoUsesFallback ? "Fallback" : "Custom",
            primitive.material.albedoExtent.width,
            primitive.material.albedoExtent.height);
        ImGui::Text(
            "%s Normal: %s (%u x %u)",
            identifier.c_str(),
            primitive.material.normalUsesFallback ? "Fallback" : "Custom",
            primitive.material.normalExtent.width,
            primitive.material.normalExtent.height);
        if(index + 1U < m_scene.primitives.size()) {
            ImGui::Separator();
        }
    }
    ImGui::End();

    update_global_uniforms(m_currentFrame);
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
    const std::array<VkDescriptorSetLayout, 2U> descriptorLayouts {
        m_descriptorSetLayout,
        m_materialDescriptorSetLayout
    };
    m_pipeline.recreate(m_context, m_renderPass, descriptorLayouts);
    m_framebuffers.recreate(m_context, m_swapchain, m_renderPass, m_depthResources.image_views());
    const std::uint32_t commandBufferCount = static_cast<std::uint32_t>(m_framebuffers.size());
    m_commandAllocator.recreate(m_context, commandBufferCount);
    m_imagesInFlight.assign(commandBufferCount, VK_NULL_HANDLE);
    create_render_finished_semaphores();
    ImGui_ImplVulkan_SetMinImageCount(static_cast<std::uint32_t>(m_swapchain.image_views().size()));
    update_global_uniforms(m_currentFrame);
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
    const std::uint32_t frameIndex = m_currentFrame;
    VkCommandBuffer commandBuffer = m_commandAllocator.buffers().at(imageIndex);
    if(vkResetCommandBuffer(commandBuffer, 0U) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to reset command buffer"};
    }

    VkCommandBufferBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to begin recording command buffer"};
    }

    render_shadow_pass(commandBuffer, frameIndex);

    std::array<VkClearValue, 2U> clearValues {};
    clearValues[0].color = {{clearColour[0], clearColour[1], clearColour[2], clearColour[3]}};
    clearValues[1].depthStencil = {1.0F, 0U};

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

    const VkDescriptorSet globalDescriptor = (frameIndex < maxFramesInFlight)
        ? m_frameResources.at(frameIndex).descriptor
        : VK_NULL_HANDLE;
    if(globalDescriptor != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipeline.layout(),
            0U,
            1U,
            &globalDescriptor,
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
        const float normalStrength = std::clamp(properties.normalStrength, 0.0F, 2.0F);
        pushConstants.materialFlags = glm::vec4 {
            properties.useAlbedoMap ? 1.0F : 0.0F,
            properties.useNormalMap ? 1.0F : 0.0F,
            normalStrength,
            0.0F
        };

        if(primitive.material.descriptor != VK_NULL_HANDLE) {
            const VkDescriptorSet materialSet = primitive.material.descriptor;
            vkCmdBindDescriptorSets(
                commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_pipeline.layout(),
                1U,
                1U,
                &materialSet,
                0U,
                nullptr);
        }

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
    planeProperties.uvScale = planeParameters.uvTiling;

    PrimitiveProperties cubeProperties {};
    cubeProperties.position = glm::vec3 {-1.0F, 0.5F, 0.0F};
    cubeProperties.scale = glm::vec3 {0.5F, 0.5F, 0.5F};
    cubeProperties.baseColor = glm::vec3 {0.9F, 0.4F, 0.1F};
    cubeProperties.uvScale = glm::vec2 {1.0F, 1.0F};

    PrimitiveProperties sphereProperties {};
    sphereProperties.position = glm::vec3 {1.0F, 0.5F, 0.0F};
    sphereProperties.scale = glm::vec3 {0.5F, 0.5F, 0.5F};
    sphereProperties.baseColor = glm::vec3 {0.3F, 0.6F, 0.9F};
    sphereProperties.uvScale = glm::vec2 {1.0F, 1.0F};

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
            scenePrimitive.material.boundAlbedo = &m_fallbackAlbedo;
            scenePrimitive.material.boundNormal = &m_fallbackNormal;
            scenePrimitive.material.albedoUsesFallback = true;
            scenePrimitive.material.normalUsesFallback = true;
            scenePrimitive.material.albedoExtent = m_fallbackAlbedoExtent;
            scenePrimitive.material.normalExtent = m_fallbackNormalExtent;
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

    std::array<VkDescriptorSetLayoutBinding, 2U> globalBindings {};

    globalBindings.at(0).binding = 0U;
    globalBindings.at(0).descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    globalBindings.at(0).descriptorCount = 1U;
    globalBindings.at(0).stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    globalBindings.at(1).binding = 1U;
    globalBindings.at(1).descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    globalBindings.at(1).descriptorCount = 1U;
    globalBindings.at(1).stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo globalLayoutInfo {};
    globalLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    globalLayoutInfo.bindingCount = static_cast<std::uint32_t>(globalBindings.size());
    globalLayoutInfo.pBindings = globalBindings.data();

    if(vkCreateDescriptorSetLayout(m_context.device(), &globalLayoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create global descriptor set layout"};
    }
    m_context.set_object_name(
        VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
        reinterpret_cast<std::uint64_t>(m_descriptorSetLayout),
        "Global Descriptor Set Layout");

    std::array<VkDescriptorSetLayoutBinding, 2U> materialBindings {};
    materialBindings.at(0).binding = 0U;
    materialBindings.at(0).descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    materialBindings.at(0).descriptorCount = 1U;
    materialBindings.at(0).stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    materialBindings.at(1).binding = 1U;
    materialBindings.at(1).descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    materialBindings.at(1).descriptorCount = 1U;
    materialBindings.at(1).stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo materialLayoutInfo {};
    materialLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    materialLayoutInfo.bindingCount = static_cast<std::uint32_t>(materialBindings.size());
    materialLayoutInfo.pBindings = materialBindings.data();

    if(vkCreateDescriptorSetLayout(m_context.device(), &materialLayoutInfo, nullptr, &m_materialDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create material descriptor set layout"};
    }
    m_context.set_object_name(
        VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
        reinterpret_cast<std::uint64_t>(m_materialDescriptorSetLayout),
        "Material Descriptor Set Layout");

    std::array<VkDescriptorPoolSize, 2U> globalPoolSizes {};
    globalPoolSizes.at(0).type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    globalPoolSizes.at(0).descriptorCount = maxFramesInFlight;
    globalPoolSizes.at(1).type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    globalPoolSizes.at(1).descriptorCount = maxFramesInFlight;

    VkDescriptorPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<std::uint32_t>(globalPoolSizes.size());
    poolInfo.pPoolSizes = globalPoolSizes.data();
    poolInfo.maxSets = maxFramesInFlight;

    if(vkCreateDescriptorPool(m_context.device(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create global descriptor pool"};
    }
    m_context.set_object_name(
        VK_OBJECT_TYPE_DESCRIPTOR_POOL,
        reinterpret_cast<std::uint64_t>(m_descriptorPool),
        "Global Descriptor Pool");

    std::array<VkDescriptorSetLayout, maxFramesInFlight> layouts {};
    layouts.fill(m_descriptorSetLayout);

    std::array<VkDescriptorSet, maxFramesInFlight> descriptors {};

    VkDescriptorSetAllocateInfo allocateInfo {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = m_descriptorPool;
    allocateInfo.descriptorSetCount = maxFramesInFlight;
    allocateInfo.pSetLayouts = layouts.data();

    if(vkAllocateDescriptorSets(m_context.device(), &allocateInfo, descriptors.data()) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to allocate global descriptor sets"};
    }

    for(std::uint32_t index {0U}; index < maxFramesInFlight; ++index) {
        FrameResources& frame = m_frameResources.at(index);
        frame.descriptor = descriptors.at(index);
        frame.uniformBuffer = Buffer::create_uniform_buffer(m_context, sizeof(GlobalUniform));
        frame.descriptorDirty = true;

        const std::string bufferName = "Global Uniform Buffer [" + std::to_string(index) + "]";
        m_context.set_object_name(
            VK_OBJECT_TYPE_BUFFER,
            reinterpret_cast<std::uint64_t>(frame.uniformBuffer.handle()),
            bufferName);
    }

    mark_all_frame_descriptors_dirty();
    update_global_uniforms(m_currentFrame);
}

void VulkanApplication::update_descriptor_set_bindings(std::uint32_t frameIndex) {
    if(frameIndex >= maxFramesInFlight) {
        return;
    }

    FrameResources& frame = m_frameResources.at(frameIndex);
    if(!frame.descriptorDirty) {
        return;
    }
    if(frame.descriptor == VK_NULL_HANDLE) {
        return;
    }
    if(frame.uniformBuffer.handle() == VK_NULL_HANDLE) {
        return;
    }

    VkDescriptorBufferInfo bufferInfo {};
    bufferInfo.buffer = frame.uniformBuffer.handle();
    bufferInfo.offset = 0U;
    bufferInfo.range = sizeof(GlobalUniform);

    std::array<VkWriteDescriptorSet, 2U> writes {};
    std::uint32_t writeCount {0U};

    writes.at(writeCount).sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes.at(writeCount).dstSet = frame.descriptor;
    writes.at(writeCount).dstBinding = 0U;
    writes.at(writeCount).descriptorCount = 1U;
    writes.at(writeCount).descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes.at(writeCount).pBufferInfo = &bufferInfo;
    ++writeCount;

    const VkSampler sampler = m_shadowMapResources.sampler();
    const VkImageView imageView = m_shadowMapResources.image_view_array();
    const VkImageLayout currentLayout = m_shadow.sampleLayout;
    const VkImageLayout descriptorLayout = (currentLayout != VK_IMAGE_LAYOUT_UNDEFINED)
        ? currentLayout
        : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    const bool imageReady = sampler != VK_NULL_HANDLE && imageView != VK_NULL_HANDLE;

    VkDescriptorImageInfo imageInfo {};
    if(imageReady) {
        imageInfo.sampler = sampler;
        imageInfo.imageView = imageView;
        imageInfo.imageLayout = descriptorLayout;

        writes.at(writeCount).sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes.at(writeCount).dstSet = frame.descriptor;
        writes.at(writeCount).dstBinding = 1U;
        writes.at(writeCount).descriptorCount = 1U;
        writes.at(writeCount).descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes.at(writeCount).pImageInfo = &imageInfo;
        ++writeCount;
    }

    vkUpdateDescriptorSets(
        m_context.device(),
        writeCount,
        writes.data(),
        0U,
        nullptr);

    const bool layoutKnown = currentLayout != VK_IMAGE_LAYOUT_UNDEFINED;
    frame.descriptorDirty = !(imageReady && layoutKnown);

    const bool anyDirty = std::any_of(
        m_frameResources.begin(),
        m_frameResources.end(),
        [](const FrameResources& candidate) {
            return candidate.descriptorDirty;
        });
    m_shadow.descriptorDirty = anyDirty;
}

void VulkanApplication::destroy_descriptor_resources() noexcept {
    destroy_material_descriptor_pool();
    destroy_texture_resources();

    for(FrameResources& frame : m_frameResources) {
        frame.uniformBuffer = Buffer {};
        frame.descriptor = VK_NULL_HANDLE;
        frame.descriptorDirty = true;
    }

    if(m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_context.device(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if(m_materialDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_context.device(), m_materialDescriptorSetLayout, nullptr);
        m_materialDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if(m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_context.device(), m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }

    m_shadow.descriptorDirty = true;
}

void VulkanApplication::mark_all_frame_descriptors_dirty() noexcept {
    for(FrameResources& frame : m_frameResources) {
        frame.descriptorDirty = true;
    }
    m_shadow.descriptorDirty = true;
}

void VulkanApplication::create_texture_resources() {
    destroy_texture_resources();

    if(m_context.device() == VK_NULL_HANDLE) {
        return;
    }

    m_textureSamplers = TextureSamplers::create(m_context);

    const TexturePixelData checkerboard = generate_checkerboard_pixels();
    m_fallbackAlbedo = create_texture_from_pixels(
        m_context,
        checkerboard,
        VK_FORMAT_R8G8B8A8_SRGB,
        "Fallback Checkerboard Albedo");
    m_fallbackAlbedoExtent = checkerboard.extent;

    const TexturePixelData normalNoise = generate_blue_noise_normal_pixels();
    m_fallbackNormal = create_texture_from_pixels(
        m_context,
        normalNoise,
        VK_FORMAT_R8G8B8A8_UNORM,
        "Fallback Blue Noise Normal");
    m_fallbackNormalExtent = normalNoise.extent;

    for(ScenePrimitive& primitive : m_scene.primitives) {
        if(primitive.material.albedoUsesFallback) {
            primitive.material.boundAlbedo = &m_fallbackAlbedo;
            primitive.material.albedoExtent = m_fallbackAlbedoExtent;
        }
        if(primitive.material.normalUsesFallback) {
            primitive.material.boundNormal = &m_fallbackNormal;
            primitive.material.normalExtent = m_fallbackNormalExtent;
        }
    }

    update_all_material_descriptors();
}

void VulkanApplication::destroy_texture_resources() noexcept {
    m_fallbackAlbedo = TextureImage {};
    m_fallbackNormal = TextureImage {};
    m_textureSamplers = TextureSamplers {};
    m_fallbackAlbedoExtent = VkExtent2D {0U, 0U};
    m_fallbackNormalExtent = VkExtent2D {0U, 0U};

    for(ScenePrimitive& primitive : m_scene.primitives) {
        if(primitive.material.albedoUsesFallback) {
            primitive.material.boundAlbedo = nullptr;
            primitive.material.albedoExtent = VkExtent2D {0U, 0U};
        }
        if(primitive.material.normalUsesFallback) {
            primitive.material.boundNormal = nullptr;
            primitive.material.normalExtent = VkExtent2D {0U, 0U};
        }
    }
}

void VulkanApplication::allocate_material_descriptor_pool(std::uint32_t primitiveCount) {
    destroy_material_descriptor_pool();

    if(primitiveCount == 0U || m_materialDescriptorSetLayout == VK_NULL_HANDLE) {
        return;
    }

    std::array<VkDescriptorPoolSize, 1U> poolSizes {};
    poolSizes.at(0).type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes.at(0).descriptorCount = primitiveCount * 2U;

    VkDescriptorPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = primitiveCount;

    if(vkCreateDescriptorPool(m_context.device(), &poolInfo, nullptr, &m_materialDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create material descriptor pool"};
    }
    m_context.set_object_name(
        VK_OBJECT_TYPE_DESCRIPTOR_POOL,
        reinterpret_cast<std::uint64_t>(m_materialDescriptorPool),
        "Material Descriptor Pool");

    for(ScenePrimitive& primitive : m_scene.primitives) {
        VkDescriptorSet descriptor {VK_NULL_HANDLE};
        VkDescriptorSetAllocateInfo allocateInfo {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = m_materialDescriptorPool;
        allocateInfo.descriptorSetCount = 1U;
        allocateInfo.pSetLayouts = &m_materialDescriptorSetLayout;

        if(vkAllocateDescriptorSets(m_context.device(), &allocateInfo, &descriptor) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to allocate material descriptor set"};
        }
        primitive.material.descriptor = descriptor;
        if(primitive.material.boundAlbedo == nullptr) {
            primitive.material.boundAlbedo = &m_fallbackAlbedo;
            primitive.material.albedoUsesFallback = true;
            primitive.material.albedoExtent = m_fallbackAlbedoExtent;
        }
        if(primitive.material.boundNormal == nullptr) {
            primitive.material.boundNormal = &m_fallbackNormal;
            primitive.material.normalUsesFallback = true;
            primitive.material.normalExtent = m_fallbackNormalExtent;
        }
    }

    update_all_material_descriptors();
}

void VulkanApplication::destroy_material_descriptor_pool() noexcept {
    if(m_materialDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_context.device(), m_materialDescriptorPool, nullptr);
        m_materialDescriptorPool = VK_NULL_HANDLE;
    }
    for(ScenePrimitive& primitive : m_scene.primitives) {
        primitive.material.descriptor = VK_NULL_HANDLE;
    }
}

void VulkanApplication::update_material_descriptor(ScenePrimitive& primitive) {
    if(primitive.material.descriptor == VK_NULL_HANDLE) {
        return;
    }
    if(m_textureSamplers.albedo() == VK_NULL_HANDLE || m_textureSamplers.normal() == VK_NULL_HANDLE) {
        return;
    }

    const TextureImage* albedoImage = primitive.material.boundAlbedo;
    if(albedoImage == nullptr || albedoImage->image_view() == VK_NULL_HANDLE) {
        albedoImage = &m_fallbackAlbedo;
        primitive.material.albedoUsesFallback = true;
        primitive.material.albedoExtent = m_fallbackAlbedoExtent;
    }

    const TextureImage* normalImage = primitive.material.boundNormal;
    if(normalImage == nullptr || normalImage->image_view() == VK_NULL_HANDLE) {
        normalImage = &m_fallbackNormal;
        primitive.material.normalUsesFallback = true;
        primitive.material.normalExtent = m_fallbackNormalExtent;
    }

    VkDescriptorImageInfo albedoInfo {};
    albedoInfo.sampler = m_textureSamplers.albedo();
    albedoInfo.imageView = albedoImage->image_view();
    albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo normalInfo {};
    normalInfo.sampler = m_textureSamplers.normal();
    normalInfo.imageView = normalImage->image_view();
    normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::array<VkWriteDescriptorSet, 2U> writes {};
    writes.at(0).sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes.at(0).dstSet = primitive.material.descriptor;
    writes.at(0).dstBinding = 0U;
    writes.at(0).descriptorCount = 1U;
    writes.at(0).descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes.at(0).pImageInfo = &albedoInfo;

    writes.at(1).sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes.at(1).dstSet = primitive.material.descriptor;
    writes.at(1).dstBinding = 1U;
    writes.at(1).descriptorCount = 1U;
    writes.at(1).descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes.at(1).pImageInfo = &normalInfo;

    vkUpdateDescriptorSets(
        m_context.device(),
        static_cast<std::uint32_t>(writes.size()),
        writes.data(),
        0U,
        nullptr);

    primitive.material.boundAlbedo = albedoImage;
    primitive.material.boundNormal = normalImage;
}

void VulkanApplication::update_all_material_descriptors() {
    for(ScenePrimitive& primitive : m_scene.primitives) {
        update_material_descriptor(primitive);
    }
}

void VulkanApplication::create_shadow_resources() {
    const std::array<VkFormat, 2U> shadowFormats {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D16_UNORM
    };

    VkFormat selectedFormat {shadowFormats.front()};
    for(VkFormat candidate : shadowFormats) {
        VkFormatProperties properties {};
        vkGetPhysicalDeviceFormatProperties(m_context.physical_device(), candidate, &properties);
        const bool supportsDepth = (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0U;
        const bool supportsSample = (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0U;
        if(supportsDepth && supportsSample) {
            selectedFormat = candidate;
            break;
        }
    }
    m_shadow.format = selectedFormat;

    const bool pipelineInitialised = m_shadowPipeline.handle() != VK_NULL_HANDLE;

    if(m_shadowRenderPass.handle() == VK_NULL_HANDLE) {
        m_shadowRenderPass = ShadowRenderPass::create(m_context, m_shadow.format);
    } else {
        m_shadowRenderPass.recreate(m_context, m_shadow.format);
        if(pipelineInitialised) {
            const std::array<VkDescriptorSetLayout, 2U> descriptorLayouts {
                m_descriptorSetLayout,
                m_materialDescriptorSetLayout
            };
            m_shadowPipeline.recreate(m_context, m_shadowRenderPass, descriptorLayouts);
        }
    }

    const std::uint32_t desiredCascades = std::clamp(m_shadow.settings.cascadeCount, 1U, maxShadowCascades);
    const std::uint32_t desiredResolution = std::max(m_shadow.settings.resolution, 1U);

    m_shadowMapResources = CascadedShadowMapResources::create(
        m_context,
        m_shadowRenderPass,
        m_shadow.format,
        desiredCascades,
        desiredResolution);

    m_shadow.cascadeCount = desiredCascades;
    m_shadow.resolution = desiredResolution;
    m_shadow.settings.cascadeCount = desiredCascades;
    m_shadow.settings.resolution = desiredResolution;
    m_shadow.sampleLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_shadow.cascadeTexelSizes.fill(0.0F);
    m_shadow.cascadeRadii.fill(0.0F);
    m_shadow.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_shadow.firstUse = true;
    mark_all_frame_descriptors_dirty();
    m_shadow.debugDescriptorsDirty = true;
}

void VulkanApplication::destroy_shadow_resources() noexcept {
    clear_shadow_debug_textures();
    m_shadowMapResources.cleanup();
    m_shadowPipeline.cleanup();
    m_shadowRenderPass.cleanup();
    m_shadow.sampleLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_shadow.cascadeTexelSizes.fill(0.0F);
    m_shadow.cascadeRadii.fill(0.0F);
    m_shadow.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_shadow.firstUse = true;
    mark_all_frame_descriptors_dirty();
    m_shadow.resourcesDirty = false;
    m_shadow.debugDescriptorsDirty = true;
}

void VulkanApplication::ensure_shadow_resources(std::uint32_t frameIndex) {
    if(m_shadow.resourcesDirty) {
        recreate_shadow_resources(m_shadow.settings.cascadeCount, m_shadow.settings.resolution);
        m_shadow.resourcesDirty = false;
    }

    if(m_shadowMapResources.image() == VK_NULL_HANDLE) {
        create_shadow_resources();
    }

    if(frameIndex < maxFramesInFlight) {
        const FrameResources& frame = m_frameResources.at(frameIndex);
        if(frame.descriptorDirty || m_shadow.descriptorDirty) {
            update_descriptor_set_bindings(frameIndex);
        }
    }

    if(m_shadowPipeline.handle() == VK_NULL_HANDLE && m_shadowRenderPass.handle() != VK_NULL_HANDLE && m_descriptorSetLayout != VK_NULL_HANDLE) {
        const std::array<VkDescriptorSetLayout, 2U> descriptorLayouts {
            m_descriptorSetLayout,
            m_materialDescriptorSetLayout
        };
        m_shadowPipeline = ShadowPipeline::create(m_context, m_shadowRenderPass, descriptorLayouts);
    }
}

void VulkanApplication::recreate_shadow_resources(std::uint32_t cascadeCount, std::uint32_t resolution) {
    const std::uint32_t clampedCascadeCount = std::clamp(cascadeCount, 1U, maxShadowCascades);
    const std::uint32_t clampedResolution = std::max(resolution, 1U);

    if(m_shadowRenderPass.handle() == VK_NULL_HANDLE || m_shadowMapResources.image() == VK_NULL_HANDLE) {
        create_shadow_resources();
    } else {
        m_shadowMapResources.recreate(
            m_context,
            m_shadowRenderPass,
            m_shadow.format,
            clampedCascadeCount,
            clampedResolution);
        m_shadow.cascadeCount = clampedCascadeCount;
        m_shadow.resolution = clampedResolution;
        m_shadow.settings.cascadeCount = clampedCascadeCount;
        m_shadow.settings.resolution = clampedResolution;
        m_shadow.sampleLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_shadow.cascadeTexelSizes.fill(0.0F);
        m_shadow.cascadeRadii.fill(0.0F);
        m_shadow.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_shadow.firstUse = true;
    }

    mark_all_frame_descriptors_dirty();
    m_shadow.debugDescriptorsDirty = true;
}

void VulkanApplication::clear_shadow_debug_textures() noexcept {
    if(ImGui::GetCurrentContext() != nullptr) {
        for(VkDescriptorSet descriptor : m_shadow.debugAtlasDescriptors) {
            if(descriptor != VK_NULL_HANDLE) {
                ImGui_ImplVulkan_RemoveTexture(descriptor);
            }
        }
    }
    m_shadow.debugAtlasDescriptors.clear();
}

void VulkanApplication::update_shadow_debug_textures() {
    if(ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    if(!m_shadow.settings.showShadowAtlas) {
        clear_shadow_debug_textures();
        m_shadow.debugDescriptorsDirty = false;
        return;
    }
    const VkSampler sampler = m_shadowMapResources.sampler();
    const VkImageLayout sampleLayout = m_shadow.sampleLayout;
    if(sampler == VK_NULL_HANDLE || sampleLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
        m_shadow.debugDescriptorsDirty = true;
        return;
    }
    clear_shadow_debug_textures();

    for(std::uint32_t index {0U}; index < m_shadow.cascadeCount; ++index) {
        const VkImageView layerView = m_shadowMapResources.layer_view(index);
        if(layerView == VK_NULL_HANDLE) {
            continue;
        }
        const VkDescriptorSet descriptor = ImGui_ImplVulkan_AddTexture(
            sampler,
            layerView,
            sampleLayout);
        if(descriptor != VK_NULL_HANDLE) {
            m_shadow.debugAtlasDescriptors.push_back(descriptor);
        }
    }

    m_shadow.debugDescriptorsDirty = false;
}

void VulkanApplication::update_global_uniforms(std::uint32_t frameIndex) {
    if(frameIndex >= maxFramesInFlight) {
        return;
    }

    FrameResources& frame = m_frameResources.at(frameIndex);
    if(frame.uniformBuffer.handle() == VK_NULL_HANDLE) {
        return;
    }

    ensure_shadow_resources(frameIndex);

    GlobalUniform uniforms {};
    const glm::vec3 cameraPos = camera_position();
    uniforms.view = compute_view_matrix(cameraPos, m_camera.yaw, m_camera.pitch, worldUp);

    const VkExtent2D extent = m_swapchain.extent();
    if(extent.height > 0U) {
        const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
        uniforms.projection = glm::perspective(m_camera.fovY, aspect, m_camera.nearPlane, m_camera.farPlane);
    }

    glm::vec3 lightDirection = m_scene.lightDirection;
    const bool validDirection = std::isfinite(lightDirection.x) && std::isfinite(lightDirection.y)
        && std::isfinite(lightDirection.z);
    if(!validDirection || glm::length(lightDirection) < movementEpsilon) {
        lightDirection = glm::normalize(glm::vec3 {0.25F, -1.0F, 0.5F});
    } else {
        lightDirection = glm::normalize(lightDirection);
    }
    m_scene.lightDirection = lightDirection;

    const glm::vec3 lightToScene = lightDirection;
    const float lightIntensity = std::max(m_scene.lightIntensity, 0.0F);
    uniforms.lightDirectionIntensity = glm::vec4 {lightToScene, lightIntensity};
    uniforms.cameraPosition = glm::vec4 {cameraPos, 1.0F};

    const VkExtent2D shadowExtent = m_shadowMapResources.extent();
    const std::uint32_t shadowResolution = (shadowExtent.width > 0U) ? shadowExtent.width : m_shadow.resolution;
    const std::uint32_t cascadeCount = std::max(1U, m_shadow.cascadeCount);

    ShadowComputationInput computationInput {};
    computationInput.view = uniforms.view;
    computationInput.projection = uniforms.projection;
    computationInput.lightDirection = lightToScene;
    computationInput.nearPlane = m_camera.nearPlane;
    computationInput.farPlane = m_camera.farPlane;

    const ShadowCascadeData cascadeData = compute_cascaded_shadow_data(
        computationInput,
        m_shadow.settings,
        cascadeCount,
        shadowResolution);

    uniforms.shadow = cascadeData.uniform;
    m_shadow.cascadeTexelSizes = cascadeData.cascadeTexelSizes;
    m_shadow.cascadeRadii = cascadeData.cascadeRadii;
    uniforms.shadow.shadowParams.w = static_cast<float>(cascadeCount);
    uniforms.shadow.atlasSize.x = static_cast<float>(shadowResolution);
    uniforms.shadow.atlasSize.y = (shadowResolution > 0U) ? (1.0F / static_cast<float>(shadowResolution)) : 0.0F;

    frame.uniformBuffer.write(m_context, &uniforms, sizeof(GlobalUniform));
}

void VulkanApplication::render_shadow_pass(VkCommandBuffer commandBuffer, std::uint32_t frameIndex) {
    ensure_shadow_resources(frameIndex);

    if(m_shadowMapResources.image() == VK_NULL_HANDLE) {
        return;
    }
    if(m_shadowPipeline.handle() == VK_NULL_HANDLE) {
        return;
    }

    const VkImage shadowImage = m_shadowMapResources.image();
    const VkExtent2D extent = m_shadowMapResources.extent();
    if(extent.width == 0U || extent.height == 0U) {
        return;
    }
    if(m_shadow.cascadeCount == 0U) {
        return;
    }
    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if(m_shadow.format == VK_FORMAT_D32_SFLOAT_S8_UINT || m_shadow.format == VK_FORMAT_D24_UNORM_S8_UINT) {
        aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    const VkViewport viewport {
        .x = 0.0F,
        .y = 0.0F,
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0.0F,
        .maxDepth = 1.0F
    };
    const VkRect2D scissor {{0, 0}, extent};

    VkImageMemoryBarrier toDepthBarrier {};
    toDepthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toDepthBarrier.srcAccessMask = m_shadow.firstUse ? 0U : VK_ACCESS_SHADER_READ_BIT;
    toDepthBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    toDepthBarrier.oldLayout = m_shadow.firstUse ? VK_IMAGE_LAYOUT_UNDEFINED : m_shadow.currentLayout;
    toDepthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    toDepthBarrier.image = shadowImage;
    toDepthBarrier.subresourceRange.aspectMask = aspectMask;
    toDepthBarrier.subresourceRange.baseMipLevel = 0U;
    toDepthBarrier.subresourceRange.levelCount = 1U;
    toDepthBarrier.subresourceRange.baseArrayLayer = 0U;
    toDepthBarrier.subresourceRange.layerCount = m_shadow.cascadeCount;

    const VkPipelineStageFlags srcStage = m_shadow.firstUse ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    const VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        srcStage,
        dstStage,
        0U,
        0U,
        nullptr,
        0U,
        nullptr,
        1U,
        &toDepthBarrier);

    VkClearValue clearValue {};
    clearValue.depthStencil.depth = 1.0F;
    clearValue.depthStencil.stencil = 0U;
    const float biasConstant = m_shadow.settings.biasConstant;
    const float biasSlope = m_shadow.settings.biasSlope;

    for(std::uint32_t cascadeIndex {0U}; cascadeIndex < m_shadow.cascadeCount; ++cascadeIndex) {
        VkRenderPassBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.renderPass = m_shadowRenderPass.handle();
        beginInfo.framebuffer = m_shadowMapResources.framebuffer(cascadeIndex);
        beginInfo.renderArea.offset = {0, 0};
        beginInfo.renderArea.extent = extent;
        beginInfo.clearValueCount = 1U;
        beginInfo.pClearValues = &clearValue;

        const std::string label = "Shadow Cascade " + std::to_string(cascadeIndex);
        m_context.begin_debug_label(commandBuffer, label);
        vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.handle());
        if(frameIndex < maxFramesInFlight) {
            const VkDescriptorSet descriptor = m_frameResources.at(frameIndex).descriptor;
            if(descriptor != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(
                    commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_shadowPipeline.layout(),
                    0U,
                    1U,
                    &descriptor,
                    0U,
                    nullptr);
            }
        }

        vkCmdSetViewport(commandBuffer, 0U, 1U, &viewport);
        vkCmdSetScissor(commandBuffer, 0U, 1U, &scissor);
        const float texelSize = (cascadeIndex < m_shadow.cascadeTexelSizes.size())
            ? std::max(m_shadow.cascadeTexelSizes.at(cascadeIndex), 1.0F)
            : 1.0F;
        const float scaledConstant = std::clamp(biasConstant * texelSize, biasConstant, biasConstant * 8.0F);
        const float scaledSlope = std::clamp(biasSlope * texelSize, biasSlope, biasSlope * 8.0F);
        vkCmdSetDepthBias(commandBuffer, scaledConstant, 0.0F, scaledSlope);

        for(ScenePrimitive& primitive : m_scene.primitives) {
            if(primitive.primitive == nullptr || !primitive.gpu.has_geometry()) {
                continue;
            }

            MeshGpuResources& gpu = primitive.gpu;

            const VkBuffer vertexBuffers[] {gpu.vertex_buffer()};
            const VkDeviceSize offsets[] {0U};
            vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, gpu.index_buffer(), 0U, VK_INDEX_TYPE_UINT32);

            ShadowPushConstants pushConstants {};
            pushConstants.model = compute_model_matrix(primitive.primitive->properties());
            pushConstants.cascadeIndex = cascadeIndex;

            vkCmdPushConstants(
                commandBuffer,
                m_shadowPipeline.layout(),
                VK_SHADER_STAGE_VERTEX_BIT,
                0U,
                static_cast<std::uint32_t>(sizeof(ShadowPushConstants)),
                &pushConstants);

            vkCmdDrawIndexed(commandBuffer, gpu.index_count(), 1U, 0U, 0, 0);
        }

        vkCmdEndRenderPass(commandBuffer);
        m_context.end_debug_label(commandBuffer);
    }

    VkImageMemoryBarrier toSampleBarrier {};
    toSampleBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toSampleBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    toSampleBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toSampleBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    const VkImageLayout sampleLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    toSampleBarrier.newLayout = sampleLayout;
    toSampleBarrier.image = shadowImage;
    toSampleBarrier.subresourceRange.aspectMask = aspectMask;
    toSampleBarrier.subresourceRange.baseMipLevel = 0U;
    toSampleBarrier.subresourceRange.levelCount = 1U;
    toSampleBarrier.subresourceRange.baseArrayLayer = 0U;
    toSampleBarrier.subresourceRange.layerCount = m_shadow.cascadeCount;

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
        &toSampleBarrier);

    m_shadow.sampleLayout = sampleLayout;
    m_shadow.currentLayout = sampleLayout;
    m_shadow.firstUse = false;
    mark_all_frame_descriptors_dirty();
    m_shadow.debugDescriptorsDirty = true;
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
