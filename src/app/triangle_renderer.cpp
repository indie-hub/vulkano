#include <vulkano/app/triangle_renderer.hpp>

#include <vulkano/app/vulkan_context.hpp>
#include <vulkano/app/window.hpp>
#include <vulkano/app/math.hpp>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Vertex final {
    glm::vec3 position {};
    glm::vec3 color {};
};

[[nodiscard]] VkVertexInputBindingDescription vertex_binding_description() noexcept {
    VkVertexInputBindingDescription binding {};
    binding.binding = 0U;
    binding.stride = static_cast<std::uint32_t>(sizeof(Vertex));
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding;
}

[[nodiscard]] std::array<VkVertexInputAttributeDescription, 2> vertex_attribute_descriptions() noexcept {
    std::array<VkVertexInputAttributeDescription, 2> attributes {};
    attributes[0].location = 0U;
    attributes[0].binding = 0U;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = static_cast<std::uint32_t>(offsetof(Vertex, position));

    attributes[1].location = 1U;
    attributes[1].binding = 0U;
    attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset = static_cast<std::uint32_t>(offsetof(Vertex, color));
    return attributes;
}

[[nodiscard]] std::vector<char> read_binary_file(const std::filesystem::path& path) {
    std::ifstream file {path, std::ios::binary | std::ios::ate};
    if (!file.is_open()) {
        throw std::runtime_error {"Unable to open shader file: " + path.string()};
    }

    const std::streamsize fileSize = file.tellg();
    std::vector<char> buffer(static_cast<std::size_t>(fileSize));
    file.seekg(0);
    if (!file.read(buffer.data(), fileSize)) {
        throw std::runtime_error {"Failed to read shader file: " + path.string()};
    }
    return buffer;
}

[[nodiscard]] VkShaderModule create_shader_module(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const std::uint32_t*>(code.data());

    VkShaderModule shaderModule {VK_NULL_HANDLE};
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create shader module"};
    }
    return shaderModule;
}

[[nodiscard]] std::filesystem::path shader_directory() {
    const std::filesystem::path current = std::filesystem::current_path();
    return current / "bin" / "shaders";
}

[[nodiscard]] std::uint32_t find_memory_type(VkPhysicalDevice physicalDevice, std::uint32_t typeFilter,
    VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memoryProperties {};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    for (std::uint32_t i {0U}; i < memoryProperties.memoryTypeCount; ++i) {
        const bool typeSupported = (typeFilter & (1U << i)) != 0U;
        const bool flagsSupported = (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties;
        if (typeSupported && flagsSupported) {
            return i;
        }
    }

    throw std::runtime_error {"Failed to find suitable memory type"};
}

[[nodiscard]] std::array<Vertex, 3> triangle_vertices() noexcept {
    return {
        Vertex {glm::vec3 {-0.5F, -0.5F, 0.0F}, glm::vec3 {1.0F, 1.0F, 1.0F}},
        Vertex {glm::vec3 {0.5F, -0.5F, 0.0F}, glm::vec3 {1.0F, 1.0F, 1.0F}},
        Vertex {glm::vec3 {0.0F, 0.5F, 0.0F}, glm::vec3 {1.0F, 1.0F, 1.0F}}
    };
}
} // namespace

namespace vulkano::app {
TriangleRenderer::TriangleRenderer(const VulkanContext& context, const Window& window)
    : m_context {context} {
    create_render_pass();
    create_pipeline_layout();
    create_graphics_pipeline();
    create_framebuffers();
    create_vertex_buffer();
    configure_push_constants(window);
}

TriangleRenderer::~TriangleRenderer() noexcept {
    if (m_vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_context.device(), m_vertexBuffer, nullptr);
        m_vertexBuffer = VK_NULL_HANDLE;
    }

    if (m_vertexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_context.device(), m_vertexMemory, nullptr);
        m_vertexMemory = VK_NULL_HANDLE;
    }

    if (!m_framebuffers.empty()) {
        for (VkFramebuffer framebuffer : m_framebuffers) {
            if (framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(m_context.device(), framebuffer, nullptr);
            }
        }
        m_framebuffers.clear();
    }

    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_context.device(), m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }

    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_context.device(), m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }

    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_context.device(), m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
}

VkRenderPass TriangleRenderer::render_pass() const noexcept {
    return m_renderPass;
}

VkPipeline TriangleRenderer::pipeline() const noexcept {
    return m_pipeline;
}

VkPipelineLayout TriangleRenderer::pipeline_layout() const noexcept {
    return m_pipelineLayout;
}

const std::vector<VkFramebuffer>& TriangleRenderer::framebuffers() const noexcept {
    return m_framebuffers;
}

TrianglePushConstants TriangleRenderer::push_constants() const noexcept {
    return m_pushConstants;
}

VkBuffer TriangleRenderer::vertex_buffer() const noexcept {
    return m_vertexBuffer;
}

std::uint32_t TriangleRenderer::vertex_count() const noexcept {
    return m_vertexCount;
}

void TriangleRenderer::record_command_buffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex, const CommandRecorder& overlayRecorder) const {
    VkCommandBufferBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to begin recording command buffer"};
    }

    VkClearValue clearValue {};
    clearValue.color = {{0.0F, 0.0F, 0.0F, 1.0F}};
    const std::array<VkClearValue, 1> clearValues {clearValue};

    VkRenderPassBeginInfo renderPassInfo {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_context.swapchain_extent();
    renderPassInfo.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    const VkViewport viewport {
        .x = 0.0F,
        .y = 0.0F,
        .width = static_cast<float>(m_context.swapchain_extent().width),
        .height = static_cast<float>(m_context.swapchain_extent().height),
        .minDepth = 0.0F,
        .maxDepth = 1.0F};
    vkCmdSetViewport(commandBuffer, 0U, 1U, &viewport);

    const VkRect2D scissor {
        .offset = {0, 0},
        .extent = m_context.swapchain_extent()};
    vkCmdSetScissor(commandBuffer, 0U, 1U, &scissor);

    const VkDeviceSize offsets[] = {0U};
    vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, &m_vertexBuffer, offsets);

    const TrianglePushConstants pushConstants = m_pushConstants;
    vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0U,
        static_cast<std::uint32_t>(sizeof(TrianglePushConstants)), &pushConstants);

    vkCmdDraw(commandBuffer, m_vertexCount, 1U, 0U, 0U);

    if (overlayRecorder) {
        overlayRecorder(commandBuffer);
    }

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to record command buffer"};
    }
}

void TriangleRenderer::create_render_pass() {
    VkAttachmentDescription colorAttachment {};
    colorAttachment.format = m_context.swapchain_image_format();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef {};
    colorAttachmentRef.attachment = 0U;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1U;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0U;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0U;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1U;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1U;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1U;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_context.device(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create render pass"};
    }
}

void TriangleRenderer::create_pipeline_layout() {
    VkPushConstantRange pushConstantRange {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0U;
    pushConstantRange.size = static_cast<std::uint32_t>(sizeof(TrianglePushConstants));

    VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0U;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 1U;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(m_context.device(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create pipeline layout"};
    }
}

void TriangleRenderer::create_graphics_pipeline() {
    const std::filesystem::path shadersPath = shader_directory();
    const std::vector<char> vertexShaderCode = read_binary_file(shadersPath / "triangle.vert.spv");
    const std::vector<char> fragmentShaderCode = read_binary_file(shadersPath / "triangle.frag.spv");

    VkShaderModule vertexShaderModule = create_shader_module(m_context.device(), vertexShaderCode);
    VkShaderModule fragmentShaderModule = create_shader_module(m_context.device(), fragmentShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertexShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragmentShaderModule;
    fragShaderStageInfo.pName = "main";

    const VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    const VkVertexInputBindingDescription bindingDescription = vertex_binding_description();
    const std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions = vertex_attribute_descriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1U;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport {};
    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = static_cast<float>(m_context.swapchain_extent().width);
    viewport.height = static_cast<float>(m_context.swapchain_extent().height);
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    VkRect2D scissor {};
    scissor.offset = {0, 0};
    scissor.extent = m_context.swapchain_extent();

    VkPipelineViewportStateCreateInfo viewportState {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1U;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1U;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0F;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1U;
    colorBlending.pAttachments = &colorBlendAttachment;

    const std::array<VkDynamicState, 2> dynamicStates {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkGraphicsPipelineCreateInfo pipelineInfo {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2U;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0U;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(m_context.device(), VK_NULL_HANDLE, 1U, &pipelineInfo, nullptr, &m_pipeline)
        != VK_SUCCESS) {
        vkDestroyShaderModule(m_context.device(), fragmentShaderModule, nullptr);
        vkDestroyShaderModule(m_context.device(), vertexShaderModule, nullptr);
        throw std::runtime_error {"Failed to create graphics pipeline"};
    }

    vkDestroyShaderModule(m_context.device(), fragmentShaderModule, nullptr);
    vkDestroyShaderModule(m_context.device(), vertexShaderModule, nullptr);
}

void TriangleRenderer::create_framebuffers() {
    const std::vector<VkImageView>& imageViews = m_context.swapchain_image_views();
    m_framebuffers.resize(imageViews.size());

    for (std::size_t i {0U}; i < imageViews.size(); ++i) {
        VkImageView attachments[] = {imageViews[i]};

        VkFramebufferCreateInfo framebufferInfo {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = 1U;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = m_context.swapchain_extent().width;
        framebufferInfo.height = m_context.swapchain_extent().height;
        framebufferInfo.layers = 1U;

        if (vkCreateFramebuffer(m_context.device(), &framebufferInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create framebuffer"};
        }
    }
}

void TriangleRenderer::create_vertex_buffer() {
    const std::array<Vertex, 3> vertices = triangle_vertices();
    m_vertexCount = static_cast<std::uint32_t>(vertices.size());

    VkBufferCreateInfo bufferInfo {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(Vertex) * vertices.size();
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_context.device(), &bufferInfo, nullptr, &m_vertexBuffer) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create vertex buffer"};
    }

    VkMemoryRequirements memoryRequirements {};
    vkGetBufferMemoryRequirements(m_context.device(), m_vertexBuffer, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = find_memory_type(m_context.physical_device(), memoryRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(m_context.device(), &allocateInfo, nullptr, &m_vertexMemory) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to allocate vertex buffer memory"};
    }

    if (vkBindBufferMemory(m_context.device(), m_vertexBuffer, m_vertexMemory, 0U) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to bind vertex buffer memory"};
    }

    void* data = nullptr;
    if (vkMapMemory(m_context.device(), m_vertexMemory, 0U, bufferInfo.size, 0U, &data) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to map vertex buffer memory"};
    }

    std::memcpy(data, vertices.data(), static_cast<std::size_t>(bufferInfo.size));
    vkUnmapMemory(m_context.device(), m_vertexMemory);
}

void TriangleRenderer::configure_push_constants(const Window& window) noexcept {
    const VkExtent2D extent = window.framebuffer_extent();
    const float aspect = extent.height == 0U ? 1.0F : static_cast<float>(extent.width) / static_cast<float>(extent.height);

    const TriangleTransforms transforms = make_triangle_transforms(aspect);
    m_pushConstants.model = transforms.model;
    m_pushConstants.view = transforms.view;
    m_pushConstants.projection = transforms.projection;
}
} // namespace vulkano::app
