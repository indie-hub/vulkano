#include <vulkano/app/shadow_pass.hpp>

#include <vulkano/app/shadow_map.hpp>
#include <vulkano/app/vulkan_context.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <glm/mat4x4.hpp>

namespace {
struct PushConstants final {
    glm::mat4 model;
    glm::mat4 lightViewProjection;
};

struct Vertex final {
    glm::vec3 position {};
    glm::vec3 normal {};
    glm::vec3 color {};
    glm::vec2 uv {};
    glm::vec3 tangent {};
    float bitangentSign {1.0F};
};

[[nodiscard]] VkVertexInputBindingDescription binding_description() noexcept {
    VkVertexInputBindingDescription binding {};
    binding.binding = 0U;
    binding.stride = static_cast<std::uint32_t>(sizeof(Vertex));
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding;
}

[[nodiscard]] std::array<VkVertexInputAttributeDescription, 1> attribute_description() noexcept {
    std::array<VkVertexInputAttributeDescription, 1> attributes {};
    attributes[0].location = 0U;
    attributes[0].binding = 0U;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = static_cast<std::uint32_t>(offsetof(Vertex, position));
    return attributes;
}

[[nodiscard]] std::vector<char> read_shader(const std::filesystem::path& path) {
    std::ifstream file {path, std::ios::binary | std::ios::ate};
    if (!file.is_open()) {
        throw std::runtime_error {"Unable to open shader file: " + path.string()};
    }
    const std::streamsize size = file.tellg();
    std::vector<char> buffer(static_cast<std::size_t>(size));
    file.seekg(0);
    if (!file.read(buffer.data(), size)) {
        throw std::runtime_error {"Failed to read shader file: " + path.string()};
    }
    return buffer;
}

[[nodiscard]] VkShaderModule create_shader_module(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();
    info.pCode = reinterpret_cast<const std::uint32_t*>(code.data());

    VkShaderModule module {VK_NULL_HANDLE};
    if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create shadow shader module"};
    }
    return module;
}
} // namespace

namespace vulkano::app {
ShadowPass::ShadowPass(const VulkanContext& context, VkRenderPass renderPass) {
    create_pipeline(context, renderPass);
}

ShadowPass::~ShadowPass() noexcept {
    destroy();
}

ShadowPass::ShadowPass(ShadowPass&& other) noexcept
    : m_context {other.m_context}
    , m_pipelineLayout {other.m_pipelineLayout}
    , m_pipeline {other.m_pipeline} {
    other.m_context = nullptr;
    other.m_pipelineLayout = VK_NULL_HANDLE;
    other.m_pipeline = VK_NULL_HANDLE;
}

ShadowPass& ShadowPass::operator=(ShadowPass&& other) noexcept {
    if (this != &other) {
        destroy();
        m_context = other.m_context;
        m_pipelineLayout = other.m_pipelineLayout;
        m_pipeline = other.m_pipeline;
        other.m_context = nullptr;
        other.m_pipelineLayout = VK_NULL_HANDLE;
        other.m_pipeline = VK_NULL_HANDLE;
    }
    return *this;
}

void ShadowPass::recreate(const VulkanContext& context, VkRenderPass renderPass) {
    destroy();
    create_pipeline(context, renderPass);
}

void ShadowPass::record(VkCommandBuffer commandBuffer, const ShadowMap& shadowMap,
    const glm::mat4& lightViewProjection, const std::vector<ShadowDrawCommand>& commands) const {
    if (m_pipeline == VK_NULL_HANDLE || m_pipelineLayout == VK_NULL_HANDLE) {
        throw std::logic_error {"ShadowPass pipeline not initialised"};
    }

    VkClearValue clearValue {};
    clearValue.depthStencil = VkClearDepthStencilValue {1.0F, 0U};

    VkRenderPassBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = shadowMap.render_pass();
    beginInfo.framebuffer = shadowMap.framebuffer();
    beginInfo.renderArea.offset = VkOffset2D {0, 0};
    beginInfo.renderArea.extent = shadowMap.extent();
    beginInfo.clearValueCount = 1U;
    beginInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport {};
    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = static_cast<float>(shadowMap.extent().width);
    viewport.height = static_cast<float>(shadowMap.extent().height);
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;
    vkCmdSetViewport(commandBuffer, 0U, 1U, &viewport);

    VkRect2D scissor {VkOffset2D {0, 0}, shadowMap.extent()};
    vkCmdSetScissor(commandBuffer, 0U, 1U, &scissor);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    for (const ShadowDrawCommand& command : commands) {
        if (command.indexCount == 0U) {
            continue;
        }

        const VkBuffer vertexBuffers[] = {command.vertexBuffer};
        const VkDeviceSize offsets[] = {command.vertexOffset};
        vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, command.indexBuffer, command.indexOffset, VK_INDEX_TYPE_UINT32);

        PushConstants push {}; // NOLINT(cppcoreguidelines-pro-type-member-init)
        push.model = command.model;
        push.lightViewProjection = lightViewProjection;

        vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0U,
            static_cast<std::uint32_t>(sizeof(PushConstants)), &push);

        vkCmdDrawIndexed(commandBuffer, command.indexCount, 1U, 0U, 0, 0U);
    }

    vkCmdEndRenderPass(commandBuffer);
}

void ShadowPass::destroy() noexcept {
    if (m_context == nullptr) {
        return;
    }
    VkDevice device = m_context->device();
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    m_context = nullptr;
}

void ShadowPass::create_pipeline(const VulkanContext& context, VkRenderPass renderPass) {
    VkDevice device = context.device();
    m_context = &context;

    const std::filesystem::path shaderBase = std::filesystem::current_path() / "bin" / "shaders";
    const std::vector<char> vertCode = read_shader(shaderBase / "shadow.vert.spv");
    const std::vector<char> fragCode = read_shader(shaderBase / "shadow.frag.spv");

    VkShaderModule vertModule = create_shader_module(device, vertCode);
    VkShaderModule fragModule = create_shader_module(device, fragCode);

    VkPipelineShaderStageCreateInfo vertStage {};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage {};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "main";

    const VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

    const VkVertexInputBindingDescription binding = binding_description();
    const auto attributes = attribute_description();

    VkPipelineVertexInputStateCreateInfo vertexInput {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1U;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1U;
    viewportState.scissorCount = 1U;

    VkPipelineRasterizationStateCreateInfo rasterizer {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_TRUE;
    rasterizer.depthBiasConstantFactor = 2.5F;
    rasterizer.depthBiasSlopeFactor = 1.5F;
    rasterizer.lineWidth = 1.0F;

    VkPipelineMultisampleStateCreateInfo multisampling {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    const std::array<VkDynamicState, 2> dynamicStates {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pushRange {};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0U;
    pushRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo layoutInfo {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 1U;
    layoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
        throw std::runtime_error {"Failed to create shadow pipeline layout"};
    }

    VkGraphicsPipelineCreateInfo pipelineInfo {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2U;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0U;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1U, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
        throw std::runtime_error {"Failed to create shadow pipeline"};
    }

    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);
}

} // namespace vulkano::app
