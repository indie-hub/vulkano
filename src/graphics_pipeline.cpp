#include <vulkano/graphics_pipeline.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace {
    const std::filesystem::path shaderDirectory {"shaders"};
    const std::filesystem::path vertexShaderFile {"mesh.vert.spv"};
    const std::filesystem::path fragmentShaderFile {"mesh.frag.spv"};

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
            throw std::runtime_error {"Failed to create shader module"};
        }
        return module;
    }
}

namespace vulkano {

GraphicsPipeline::GraphicsPipeline(
    const VulkanContext& context,
    const RenderPass& renderPass,
    std::span<const VkDescriptorSetLayout> descriptorSetLayouts) {
    initialise(context, renderPass, descriptorSetLayouts);
}

GraphicsPipeline::GraphicsPipeline(GraphicsPipeline&& other) noexcept {
    move_from(std::move(other));
}

auto GraphicsPipeline::operator=(GraphicsPipeline&& other) noexcept -> GraphicsPipeline& {
    if(this != &other) {
        cleanup();
        move_from(std::move(other));
    }
    return *this;
}

GraphicsPipeline::~GraphicsPipeline() noexcept {
    cleanup();
}

auto GraphicsPipeline::create(
    const VulkanContext& context,
    const RenderPass& renderPass,
    std::span<const VkDescriptorSetLayout> descriptorSetLayouts) -> GraphicsPipeline {
    return GraphicsPipeline {context, renderPass, descriptorSetLayouts};
}

void GraphicsPipeline::recreate(
    const VulkanContext& context,
    const RenderPass& renderPass,
    std::span<const VkDescriptorSetLayout> descriptorSetLayouts) {
    cleanup();
    initialise(context, renderPass, descriptorSetLayouts);
}

auto GraphicsPipeline::handle() const noexcept -> VkPipeline {
    return m_pipeline;
}

auto GraphicsPipeline::layout() const noexcept -> VkPipelineLayout {
    return m_layout;
}

void GraphicsPipeline::initialise(
    const VulkanContext& context,
    const RenderPass& renderPass,
    std::span<const VkDescriptorSetLayout> descriptorSetLayouts) {
    m_device = context.device();

    const auto vertexPath = shaderDirectory / vertexShaderFile;
    const auto fragmentPath = shaderDirectory / fragmentShaderFile;
    const auto vertexCode = load_shader_code(vertexPath);
    const auto fragmentCode = load_shader_code(fragmentPath);

    VkShaderModule vertexModule = create_shader_module(m_device, vertexCode);
    VkShaderModule fragmentModule = create_shader_module(m_device, fragmentCode);

    VkPipelineShaderStageCreateInfo vertexStage {};
    vertexStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStage.module = vertexModule;
    vertexStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragmentStage {};
    fragmentStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentStage.module = fragmentModule;
    fragmentStage.pName = "main";

    const VkPipelineShaderStageCreateInfo shaderStages[] {vertexStage, fragmentStage};

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

    std::array<VkDynamicState, 2U> dynamicStates {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
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
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.lineWidth = 1.0F;

    VkPipelineMultisampleStateCreateInfo multisampling {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1U;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPipelineDepthStencilStateCreateInfo depthStencil {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPushConstantRange pushConstantRange {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0U;
    pushConstantRange.size = 0U; // placeholder

    VkPipelineLayoutCreateInfo layoutInfo {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = static_cast<std::uint32_t>(descriptorSetLayouts.size());
    layoutInfo.pSetLayouts = descriptorSetLayouts.empty() ? nullptr : descriptorSetLayouts.data();
    const std::uint32_t pushConstantSize = static_cast<std::uint32_t>(sizeof(glm::mat4) + (3U * sizeof(glm::vec4)));
    pushConstantRange.size = pushConstantSize;
    layoutInfo.pushConstantRangeCount = 1U;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    if(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
        vkDestroyShaderModule(m_device, fragmentModule, nullptr);
        vkDestroyShaderModule(m_device, vertexModule, nullptr);
        throw std::runtime_error {"Failed to create pipeline layout"};
    }

    VkGraphicsPipelineCreateInfo pipelineInfo {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2U;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicStateInfo;
    pipelineInfo.layout = m_layout;
    pipelineInfo.renderPass = renderPass.handle();
    pipelineInfo.subpass = 0U;

    if(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1U, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(m_device, fragmentModule, nullptr);
        vkDestroyShaderModule(m_device, vertexModule, nullptr);
        throw std::runtime_error {"Failed to create graphics pipeline"};
    }

    vkDestroyShaderModule(m_device, fragmentModule, nullptr);
    vkDestroyShaderModule(m_device, vertexModule, nullptr);

    context.set_object_name(VK_OBJECT_TYPE_PIPELINE_LAYOUT, reinterpret_cast<std::uint64_t>(m_layout), "Mesh Pipeline Layout");
    context.set_object_name(VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<std::uint64_t>(m_pipeline), "Mesh Pipeline");
}

void GraphicsPipeline::cleanup() noexcept {
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

void GraphicsPipeline::move_from(GraphicsPipeline&& other) noexcept {
    m_device = other.m_device;
    m_layout = other.m_layout;
    m_pipeline = other.m_pipeline;

    other.m_device = VK_NULL_HANDLE;
    other.m_layout = VK_NULL_HANDLE;
    other.m_pipeline = VK_NULL_HANDLE;
}

} // namespace vulkano
