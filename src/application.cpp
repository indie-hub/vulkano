#include <vulkano/application.hpp>

#include <array>
#include <cmath>
#include <memory>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

namespace {
    constexpr std::uint32_t maxFramesInFlight {2U};
    constexpr std::array<float, 4U> clearColour {0.0F, 0.0F, 0.0F, 1.0F};
    constexpr float timingSmoothingFactor {0.1F};

    struct GlobalUniform final {
        glm::mat4 view {1.0F};
        glm::mat4 projection {1.0F};
        glm::vec4 lightPositionIntensity {0.0F, 0.0F, 0.0F, 1.0F};
        glm::vec4 cameraPosition {0.0F, 0.0F, 0.0F, 1.0F};
    };

    struct PrimitivePushConstants final {
        glm::mat4 model {1.0F};
        glm::vec4 materialColor {1.0F, 1.0F, 1.0F, 1.0F};
        glm::vec4 materialProperties {32.0F, 0.1F, 0.5F, 0.0F};
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
    rebuild_dirty_meshes();
    update_global_uniforms();

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

    VkDescriptorSetLayoutCreateInfo layoutInfo {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1U;
    layoutInfo.pBindings = &uniformBinding;

    if(vkCreateDescriptorSetLayout(m_context.device(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create descriptor set layout"};
    }
    m_context.set_object_name(
        VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
        reinterpret_cast<std::uint64_t>(m_descriptorSetLayout),
        "Global Descriptor Set Layout");

    VkDescriptorPoolSize poolSize {};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1U;

    VkDescriptorPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1U;
    poolInfo.pPoolSizes = &poolSize;
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

    VkWriteDescriptorSet write {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptorSet;
    write.dstBinding = 0U;
    write.dstArrayElement = 0U;
    write.descriptorCount = 1U;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_context.device(), 1U, &write, 0U, nullptr);

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
    const glm::vec3 up {0.0F, 1.0F, 0.0F};
    uniforms.view = glm::lookAt(cameraPos, m_camera.target, up);

    const VkExtent2D extent = m_swapchain.extent();
    if(extent.height > 0U) {
        const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
        uniforms.projection = glm::perspective(m_camera.fovY, aspect, m_camera.nearPlane, m_camera.farPlane);
        uniforms.projection[1][1] *= -1.0F;
    }

    uniforms.lightPositionIntensity = glm::vec4 {m_scene.lightPosition, m_scene.lightIntensity};
    uniforms.cameraPosition = glm::vec4 {cameraPos, 1.0F};

    m_globalUniformBuffer.write(m_context, &uniforms, sizeof(GlobalUniform));
}

auto VulkanApplication::camera_position() const noexcept -> glm::vec3 {
    const float cosPitch = std::cos(m_camera.pitch);
    const float sinPitch = std::sin(m_camera.pitch);
    const float cosYaw = std::cos(m_camera.yaw);
    const float sinYaw = std::sin(m_camera.yaw);

    const float x = m_camera.target.x + (m_camera.distance * cosPitch * cosYaw);
    const float y = m_camera.target.y + (m_camera.distance * sinPitch);
    const float z = m_camera.target.z + (m_camera.distance * cosPitch * sinYaw);
    return glm::vec3 {x, y, z};
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
