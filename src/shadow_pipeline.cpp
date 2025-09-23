#include <vulkano/shadow_pipeline.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <vulkano/vertex.hpp>

namespace {
    const std::filesystem::path shaderDirectory {"shaders"};
    const std::filesystem::path shadowVertexShaderFile {"shadow_depth.vert.spv"};

    auto load_shader_code(const std::filesystem::path& path) -> std::vector<std::uint32_t> {
        std::ifstream file {path, std::ios::ate | std::ios::binary};
        if(!file.is_open()) {
            throw std::runtime_error {"Failed to open shader file: " + path.string()};
        }
        const auto fileSize = static_cast<std::size_t>(file.tellg());
        std::vector<std::uint32_t> buffer(fileSize / sizeof(std::uint32_t));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(fileSize));
        return buffer;
    }

    auto create_shader_module(VkDevice device, const std::vector<std::uint32_t>& code) -> VkShaderModule {
        VkShaderModuleCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size() * sizeof(std::uint32_t);
        createInfo.pCode = code.data();

        VkShaderModule module {VK_NULL_HANDLE};
        if(vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create shadow shader module"};
        }
        return module;
    }
}

namespace vulkano {

ShadowPushConstants::ShadowPushConstants()
    : model {1.0F}
    , cascadeIndex {0U}
    , padding0 {0.0F}
    , padding1 {0.0F}
    , padding2 {0.0F} {
}

ShadowPipeline::ShadowPipeline(
    const VulkanContext& context,
    const ShadowRenderPass& renderPass,
    std::span<const VkDescriptorSetLayout> descriptorSetLayouts) {
    initialise(context, renderPass, descriptorSetLayouts);
}

ShadowPipeline::ShadowPipeline(ShadowPipeline&& other) noexcept {
    move_from(std::move(other));
}

auto ShadowPipeline::operator=(ShadowPipeline&& other) noexcept -> ShadowPipeline& {
    if(this != &other) {
        cleanup();
        move_from(std::move(other));
    }
    return *this;
}

ShadowPipeline::~ShadowPipeline() noexcept {
    cleanup();
}

auto ShadowPipeline::create(
    const VulkanContext& context,
    const ShadowRenderPass& renderPass,
    std::span<const VkDescriptorSetLayout> descriptorSetLayouts) -> ShadowPipeline {
    return ShadowPipeline {context, renderPass, descriptorSetLayouts};
}

void ShadowPipeline::recreate(
    const VulkanContext& context,
    const ShadowRenderPass& renderPass,
    std::span<const VkDescriptorSetLayout> descriptorSetLayouts) {
    cleanup();
    initialise(context, renderPass, descriptorSetLayouts);
}

void ShadowPipeline::cleanup() noexcept {
    if(m_device != VK_NULL_HANDLE && m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
    }
    if(m_device != VK_NULL_HANDLE && m_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_layout, nullptr);
    }
    m_pipeline = VK_NULL_HANDLE;
    m_layout = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
}

auto ShadowPipeline::handle() const noexcept -> VkPipeline {
    return m_pipeline;
}

auto ShadowPipeline::layout() const noexcept -> VkPipelineLayout {
    return m_layout;
}

void ShadowPipeline::initialise(
    const VulkanContext& context,
    const ShadowRenderPass& renderPass,
    std::span<const VkDescriptorSetLayout> descriptorSetLayouts) {
    m_device = context.device();

    const auto vertexPath = shaderDirectory / shadowVertexShaderFile;
    const auto vertexCode = load_shader_code(vertexPath);
    const VkShaderModule vertexModule = create_shader_module(m_device, vertexCode);

    VkPipelineShaderStageCreateInfo vertexStage {};
    vertexStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStage.module = vertexModule;
    vertexStage.pName = "main";

    const VkPipelineShaderStageCreateInfo shaderStage = vertexStage;

    const VkVertexInputBindingDescription bindingDescription = vertex_binding_description();
    const auto attributeDescriptions = vertex_attribute_descriptions();

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

    VkPipelineViewportStateCreateInfo viewportState {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1U;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1U;
    viewportState.pScissors = nullptr;

    std::array<VkDynamicState, 3U> dynamicStates {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_DEPTH_BIAS
    };

    VkPipelineDynamicStateCreateInfo dynamicStateInfo {};
    dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateInfo.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamicStateInfo.pDynamicStates = dynamicStates.data();

    VkPipelineRasterizationStateCreateInfo rasterizer {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_TRUE;
    rasterizer.depthBiasConstantFactor = 0.0F;
    rasterizer.depthBiasClamp = 0.0F;
    rasterizer.depthBiasSlopeFactor = 0.0F;
    rasterizer.lineWidth = 1.0F;

    VkPipelineMultisampleStateCreateInfo multisampling {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencil {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineLayoutCreateInfo layoutInfo {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = static_cast<std::uint32_t>(descriptorSetLayouts.size());
    layoutInfo.pSetLayouts = descriptorSetLayouts.empty() ? nullptr : descriptorSetLayouts.data();

    VkPushConstantRange pushConstantRange {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0U;
    pushConstantRange.size = static_cast<std::uint32_t>(sizeof(ShadowPushConstants));
    layoutInfo.pushConstantRangeCount = 1U;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    if(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
        vkDestroyShaderModule(m_device, vertexModule, nullptr);
        throw std::runtime_error {"Failed to create shadow pipeline layout"};
    }

    VkGraphicsPipelineCreateInfo pipelineInfo {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 1U;
    pipelineInfo.pStages = &shaderStage;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pDynamicState = &dynamicStateInfo;
    pipelineInfo.layout = m_layout;
    pipelineInfo.renderPass = renderPass.handle();
    pipelineInfo.subpass = 0U;

    if(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1U, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(m_device, vertexModule, nullptr);
        throw std::runtime_error {"Failed to create shadow graphics pipeline"};
    }

    vkDestroyShaderModule(m_device, vertexModule, nullptr);

    context.set_object_name(
        VK_OBJECT_TYPE_PIPELINE_LAYOUT,
        reinterpret_cast<std::uint64_t>(m_layout),
        "Shadow Pipeline Layout");
    context.set_object_name(
        VK_OBJECT_TYPE_PIPELINE,
        reinterpret_cast<std::uint64_t>(m_pipeline),
        "Shadow Pipeline");
}

void ShadowPipeline::move_from(ShadowPipeline&& other) noexcept {
    m_device = other.m_device;
    m_layout = other.m_layout;
    m_pipeline = other.m_pipeline;

    other.m_device = VK_NULL_HANDLE;
    other.m_layout = VK_NULL_HANDLE;
    other.m_pipeline = VK_NULL_HANDLE;
}

} // namespace vulkano
