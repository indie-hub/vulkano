#include <vulkano/app/scene_renderer.hpp>

#include <vulkano/app/vulkan_context.hpp>
#include <vulkano/app/window.hpp>
#include <vulkano/app/material_buffer.hpp>
#include <vulkano/app/material_texture_cache.hpp>
#include <vulkano/app/light_buffer.hpp>

#include <vulkano/vk/depth_format.hpp>
#include <vulkano/vk/depth_image.hpp>

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/vec4.hpp>

namespace {
struct ShaderPaths final {
    std::filesystem::path vertexPath;
    std::filesystem::path fragmentPath;
};

struct ScenePushConstants final {
    glm::mat4 model {};
    glm::mat4 view {};
    glm::mat4 projection {};
    glm::uvec4 material {0U, 0U, 0U, 0U};
    glm::vec4 camera {0.0F, 0.0F, 0.0F, 1.0F};
};

struct Vertex final {
    glm::vec3 position {};
    glm::vec3 normal {};
    glm::vec3 color {};
    glm::vec2 uv {};
};

[[nodiscard]] VkVertexInputBindingDescription vertex_binding_description() noexcept {
    VkVertexInputBindingDescription binding {};
    binding.binding = 0U;
    binding.stride = static_cast<std::uint32_t>(sizeof(Vertex));
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding;
}

[[nodiscard]] std::array<VkVertexInputAttributeDescription, 4> vertex_attribute_descriptions() noexcept {
    std::array<VkVertexInputAttributeDescription, 4> attributes {};
    attributes[0].location = 0U;
    attributes[0].binding = 0U;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = static_cast<std::uint32_t>(offsetof(Vertex, position));

    attributes[1].location = 1U;
    attributes[1].binding = 0U;
    attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset = static_cast<std::uint32_t>(offsetof(Vertex, normal));

    attributes[2].location = 2U;
    attributes[2].binding = 0U;
    attributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[2].offset = static_cast<std::uint32_t>(offsetof(Vertex, color));
    attributes[3].location = 3U;
    attributes[3].binding = 0U;
    attributes[3].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[3].offset = static_cast<std::uint32_t>(offsetof(Vertex, uv));
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

[[nodiscard]] ShaderPaths shader_paths() {
    const std::filesystem::path base = std::filesystem::current_path() / "bin" / "shaders";
    return ShaderPaths {.vertexPath = base / "scene.vert.spv", .fragmentPath = base / "scene.frag.spv"};
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

void create_buffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo bufferInfo {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create buffer"};
    }

    VkMemoryRequirements requirements {};
    vkGetBufferMemoryRequirements(device, buffer, &requirements);

    VkMemoryAllocateInfo allocateInfo {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = find_memory_type(physicalDevice, requirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocateInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to allocate buffer memory"};
    }

    if (vkBindBufferMemory(device, buffer, memory, 0U) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to bind buffer memory"};
    }
}

[[nodiscard]] std::vector<std::uint32_t> build_default_indices(const std::size_t vertexCount) {
    std::vector<std::uint32_t> indices(vertexCount);
    for (std::uint32_t i {0U}; i < static_cast<std::uint32_t>(vertexCount); ++i) {
        indices[i] = i;
    }
    return indices;
}
}

namespace vulkano::app {
SceneRenderer::SceneRenderer(const VulkanContext& context, const Window& window, VkDescriptorSetLayout ssaoLayout)
    : m_context {context}
    , m_descriptorLayout {ssaoLayout} {
    static_cast<void>(window);
    m_depthFormat = vk::DepthFormatResolver::select_depth_format(m_context.physical_device());
    create_render_pass();
    create_material_descriptors();
    create_light_descriptors();
    create_pipeline_layout();
    create_graphics_pipeline();
    create_color_resources();
    create_depth_resources();
    create_framebuffers();
    create_light_debug_mesh();

}

SceneRenderer::~SceneRenderer() noexcept {
    destroy_meshes();
    destroy_framebuffers();
    destroy_color_resources();
    destroy_depth_resources();

    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_context.device(), m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_context.device(), m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    destroy_light_descriptors();
    destroy_material_descriptors();
    destroy_light_debug_mesh();
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_context.device(), m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
}

void SceneRenderer::set_scene(const std::vector<SceneMesh>& meshes) {
    destroy_meshes();
    m_meshes.reserve(meshes.size());
    for (const SceneMesh& mesh : meshes) {
        upload_mesh(mesh);
    }
}

void SceneRenderer::set_material_resources(const MaterialBuffer& buffer, const MaterialTextureCache& textures) {
    if (m_materialDescriptorSet == VK_NULL_HANDLE) {
        throw std::logic_error {"Material descriptor set not initialised"};
    }

    const VkDescriptorBufferInfo info = buffer.descriptor_info();
    if (info.buffer == VK_NULL_HANDLE || info.range == 0U) {
        throw std::invalid_argument {"Material buffer descriptor info is invalid"};
    }

    const auto& handles = textures.handles();
    if (handles.empty()) {
        throw std::invalid_argument {"Material textures cache is empty"};
    }

    m_materialTextureInfos = textures.descriptor_infos();
    if (m_materialTextureInfos.empty()) {
        throw std::invalid_argument {"Material textures descriptors are empty"};
    }
    if (m_materialTextureInfos.size() > MaterialDescriptorBindings::maxTextureSamplers) {
        throw std::invalid_argument {"Material textures exceed descriptor capacity"};
    }

    const VkDescriptorImageInfo filler = m_materialTextureInfos.back();
    m_materialTextureInfos.resize(MaterialDescriptorBindings::maxTextureSamplers, filler);

    std::array<VkWriteDescriptorSet, 2> writes {};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_materialDescriptorSet;
    writes[0].dstBinding = MaterialDescriptorBindings::materialBufferBinding;
    writes[0].descriptorCount = 1U;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo = &info;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_materialDescriptorSet;
    writes[1].dstBinding = MaterialDescriptorBindings::textureArrayBinding;
    writes[1].descriptorCount = MaterialDescriptorBindings::maxTextureSamplers;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = m_materialTextureInfos.data();

    vkUpdateDescriptorSets(m_context.device(), static_cast<std::uint32_t>(writes.size()), writes.data(), 0U, nullptr);

    m_materialBuffer = &buffer;
}

void SceneRenderer::set_light_resources(const LightBuffer& buffer, const scene::LightRegistry& registry) {
    if (m_lightDescriptorSet == VK_NULL_HANDLE) {
        throw std::logic_error {"Light descriptor set not initialised"};
    }

    const VkDescriptorBufferInfo info = buffer.descriptor_info();
    if (info.buffer == VK_NULL_HANDLE || info.range == 0U) {
        throw std::invalid_argument {"Light buffer descriptor info is invalid"};
    }

    VkWriteDescriptorSet write {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_lightDescriptorSet;
    write.dstBinding = 0U;
    write.descriptorCount = 1U;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &info;

    vkUpdateDescriptorSets(m_context.device(), 1U, &write, 0U, nullptr);

    if (registry.size() > 0U) {
        const scene::Light& primary = registry.light(scene::LightId {0U});
        const glm::vec3 dir = primary.direction;
        m_lightDirection = glm::length(dir) > 0.0F ? glm::normalize(dir) : glm::vec3 {0.0F, -1.0F, 0.0F};
        m_lightColor = primary.color;
        m_lightIntensity = primary.intensity;

        if (m_lightDebugMesh.vertexMemory != VK_NULL_HANDLE) {
            std::array<Vertex, 5> vertices = {
                Vertex {.position = glm::vec3 {0.0F, 0.0F, 0.0F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
                    .color = m_lightColor, .uv = glm::vec2 {0.0F, 0.0F}},
                Vertex {.position = glm::vec3 {0.05F, 0.05F, -0.2F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
                    .color = m_lightColor, .uv = glm::vec2 {0.0F, 0.0F}},
                Vertex {.position = glm::vec3 {-0.05F, 0.05F, -0.2F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
                    .color = m_lightColor, .uv = glm::vec2 {0.0F, 0.0F}},
                Vertex {.position = glm::vec3 {-0.05F, -0.05F, -0.2F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
                    .color = m_lightColor, .uv = glm::vec2 {0.0F, 0.0F}},
                Vertex {.position = glm::vec3 {0.05F, -0.05F, -0.2F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
                    .color = m_lightColor, .uv = glm::vec2 {0.0F, 0.0F}}
            };

            VkDeviceSize size = static_cast<VkDeviceSize>(sizeof(Vertex) * vertices.size());
            void* mapped = nullptr;
            if (vkMapMemory(m_context.device(), m_lightDebugMesh.vertexMemory, 0U, size, 0U, &mapped) == VK_SUCCESS) {
                std::memcpy(mapped, vertices.data(), static_cast<std::size_t>(size));
                vkUnmapMemory(m_context.device(), m_lightDebugMesh.vertexMemory);
            }
        }
    }

    m_lightBuffer = &buffer;
}

void SceneRenderer::set_show_light_debug(bool enabled) noexcept {
    m_showLightDebug = enabled;
}

VkRenderPass SceneRenderer::render_pass() const noexcept {
    return m_renderPass;
}

VkPipeline SceneRenderer::pipeline() const noexcept {
    return m_pipeline;
}

VkPipelineLayout SceneRenderer::pipeline_layout() const noexcept {
    return m_pipelineLayout;
}

const std::vector<VkFramebuffer>& SceneRenderer::framebuffers() const noexcept {
    return m_framebuffers;
}

VkImageView SceneRenderer::albedo_image_view() const noexcept {
    return m_albedoImage.view();
}

VkImageView SceneRenderer::normal_image_view() const noexcept {
    return m_normalImage.view();
}

VkFormat SceneRenderer::albedo_format() const noexcept {
    return m_albedoFormat;
}

VkFormat SceneRenderer::normal_format() const noexcept {
    return m_normalFormat;
}

VkImageView SceneRenderer::linear_depth_image_view() const noexcept {
    return m_linearDepthImage.view();
}

VkFormat SceneRenderer::linear_depth_format() const noexcept {
    return m_linearDepthFormat;
}

void SceneRenderer::record_command_buffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex,
    const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPosition,
    const CommandRecorder& overlayRecorder,
    VkDescriptorSet ssaoDescriptor) const {
    VkCommandBufferBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to begin recording command buffer"};
    }

    VkClearValue swapClear {};
    swapClear.color = {{0.0F, 0.0F, 0.0F, 1.0F}};
    VkClearValue albedoClear {};
    albedoClear.color = {{0.0F, 0.0F, 0.0F, 1.0F}};
    VkClearValue normalClear {};
    normalClear.color = {{0.5F, 0.5F, 1.0F, 1.0F}};
    VkClearValue linearDepthClear {};
    linearDepthClear.color = {{0.0F, 0.0F, 0.0F, 0.0F}};
    VkClearValue depthClear {};
    depthClear.depthStencil = {1.0F, 0U};
    const std::array<VkClearValue, 5> clearValues {swapClear, albedoClear, normalClear, linearDepthClear, depthClear};

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
    if (m_descriptorLayout != VK_NULL_HANDLE && ssaoDescriptor != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0U, 1U,
            &ssaoDescriptor, 0U, nullptr);
    }
    if (m_materialDescriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
            MaterialDescriptorBindings::set, 1U, &m_materialDescriptorSet, 0U, nullptr);
    }
    if (m_lightDescriptorSet != VK_NULL_HANDLE) {
        const VkDescriptorSet lightSet = m_lightDescriptorSet;
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 2U, 1U, &lightSet, 0U,
            nullptr);
    }

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

    for (const GpuMesh& mesh : m_meshes) {
        const VkBuffer vertexBuffers[] = {mesh.vertexBuffer};
        const VkDeviceSize offsets[] = {0U};
        vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer, 0U, VK_INDEX_TYPE_UINT32);

        const ScenePushConstants pushConstants {
            .model = mesh.model,
            .view = view,
            .projection = projection,
            .material = glm::uvec4 {mesh.material.value, 0U, 0U, 0U},
            .camera = glm::vec4 {cameraPosition, 1.0F}
        };
        vkCmdPushConstants(commandBuffer, m_pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0U,
            static_cast<std::uint32_t>(sizeof(ScenePushConstants)), &pushConstants);

        vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1U, 0U, 0, 0U);
    }

    if (m_showLightDebug && m_lightDebugMesh.indexCount > 0U && m_lightBuffer != nullptr) {
        const glm::vec3 dir = glm::length(m_lightDirection) > 0.0F ? glm::normalize(m_lightDirection)
            : glm::vec3 {0.0F, -1.0F, 0.0F};
        const float length = glm::max(m_lightIntensity, 0.1F);

        glm::vec3 forward = -dir;
        glm::vec3 upReference = glm::abs(forward.y) > 0.99F ? glm::vec3 {0.0F, 0.0F, 1.0F} : glm::vec3 {0.0F, 1.0F, 0.0F};
        glm::vec3 right = glm::normalize(glm::cross(upReference, forward));
        glm::vec3 up = glm::normalize(glm::cross(forward, right));

        glm::mat4 debugModel {1.0F};
        debugModel[0] = glm::vec4(right * 0.5F, 0.0F);
        debugModel[1] = glm::vec4(up * 0.5F, 0.0F);
        debugModel[2] = glm::vec4(forward * length, 0.0F);
        debugModel[3] = glm::vec4(0.0F, 0.0F, 0.0F, 1.0F);

        const ScenePushConstants debugConstants {
            .model = debugModel,
            .view = view,
            .projection = projection,
            .material = glm::uvec4 {0U, 0U, 0U, 0U},
            .camera = glm::vec4 {cameraPosition, 1.0F}
        };

        const VkBuffer lightBuffers[] = {m_lightDebugMesh.vertexBuffer};
        const VkDeviceSize lightOffsets[] = {0U};
        vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, lightBuffers, lightOffsets);
        vkCmdBindIndexBuffer(commandBuffer, m_lightDebugMesh.indexBuffer, 0U, VK_INDEX_TYPE_UINT32);
        vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0U,
            static_cast<std::uint32_t>(sizeof(ScenePushConstants)), &debugConstants);
        vkCmdDrawIndexed(commandBuffer, m_lightDebugMesh.indexCount, 1U, 0U, 0, 0U);
    }

    if (overlayRecorder) {
        overlayRecorder(commandBuffer);
    }

    vkCmdEndRenderPass(commandBuffer);
}

void SceneRenderer::create_render_pass() {
    VkAttachmentDescription swapAttachment {};
    swapAttachment.format = m_context.swapchain_image_format();
    swapAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    swapAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    swapAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    swapAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    swapAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    swapAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription albedoAttachment {};
    albedoAttachment.format = m_albedoFormat;
    albedoAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    albedoAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    albedoAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    albedoAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    albedoAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    albedoAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    albedoAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription normalAttachment {};
    normalAttachment.format = m_normalFormat;
    normalAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    normalAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    normalAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    normalAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    normalAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    normalAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    normalAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription linearDepthAttachment {};
    linearDepthAttachment.format = m_linearDepthFormat;
    linearDepthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    linearDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    linearDepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    linearDepthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    linearDepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    linearDepthAttachment.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    linearDepthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription depthAttachment {};
    depthAttachment.format = m_depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    const std::array<VkAttachmentReference, 4> colorAttachmentRefs {
        VkAttachmentReference {0U, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        VkAttachmentReference {1U, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        VkAttachmentReference {2U, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        VkAttachmentReference {3U, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
    };

    VkAttachmentReference depthAttachmentRef {};
    depthAttachmentRef.attachment = 4U;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<std::uint32_t>(colorAttachmentRefs.size());
    subpass.pColorAttachments = colorAttachmentRefs.data();
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    std::array<VkSubpassDependency, 2> dependencies {};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0U;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = 0U;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    dependencies[1].srcSubpass = 0U;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo renderPassInfo {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    const std::array<VkAttachmentDescription, 5> attachments {
        swapAttachment,
        albedoAttachment,
        normalAttachment,
        linearDepthAttachment,
        depthAttachment
    };
    renderPassInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1U;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<std::uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(m_context.device(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create render pass"};
    }
}

void SceneRenderer::create_pipeline_layout() {
    VkPushConstantRange pushConstantRange {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0U;
    pushConstantRange.size = static_cast<std::uint32_t>(sizeof(ScenePushConstants));

    VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    std::array<VkDescriptorSetLayout, 3> layouts {};
    std::uint32_t layoutCount {0U};
    if (m_descriptorLayout == VK_NULL_HANDLE) {
        throw std::logic_error {"SceneRenderer requires SSAO descriptor layout for set 0"};
    }
    if (m_materialDescriptorLayout == VK_NULL_HANDLE) {
        throw std::logic_error {"SceneRenderer material descriptor layout not initialised"};
    }
    if (m_lightDescriptorLayout == VK_NULL_HANDLE) {
        throw std::logic_error {"SceneRenderer light descriptor layout not initialised"};
    }
    layouts[layoutCount++] = m_descriptorLayout;
    layouts[layoutCount++] = m_materialDescriptorLayout;
    layouts[layoutCount++] = m_lightDescriptorLayout;
    pipelineLayoutInfo.setLayoutCount = layoutCount;
    pipelineLayoutInfo.pSetLayouts = layouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1U;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(m_context.device(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create pipeline layout"};
    }
}

void SceneRenderer::create_graphics_pipeline() {
    const ShaderPaths paths = shader_paths();
    const VkDevice device = m_context.device();

    const std::vector<char> vertexShaderCode = read_binary_file(paths.vertexPath);
    const std::vector<char> fragmentShaderCode = read_binary_file(paths.fragmentPath);

    VkShaderModule vertexShader = create_shader_module(device, vertexShaderCode);
    VkShaderModule fragmentShader = create_shader_module(device, fragmentShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertexShader;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragmentShader;
    fragShaderStageInfo.pName = "main";

    const VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

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
    viewportState.scissorCount = 1U;

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

    VkPipelineDepthStencilStateCreateInfo depthStencil {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {};
    depthStencil.back = {};
    depthStencil.minDepthBounds = 0.0F;
    depthStencil.maxDepthBounds = 1.0F;

    VkPipelineColorBlendAttachmentState colorBlendAttachments[4] {};
    for (auto& attachment : colorBlendAttachments) {
        attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
            | VK_COLOR_COMPONENT_A_BIT;
        attachment.blendEnable = VK_FALSE;
    }

    VkPipelineColorBlendStateCreateInfo colorBlending {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 4U;
    colorBlending.pAttachments = colorBlendAttachments;

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
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0U;

    if (vkCreateGraphicsPipelines(m_context.device(), VK_NULL_HANDLE, 1U, &pipelineInfo, nullptr, &m_pipeline)
        != VK_SUCCESS) {
        vkDestroyShaderModule(device, fragmentShader, nullptr);
        vkDestroyShaderModule(device, vertexShader, nullptr);
        throw std::runtime_error {"Failed to create graphics pipeline"};
    }

    vkDestroyShaderModule(device, fragmentShader, nullptr);
    vkDestroyShaderModule(device, vertexShader, nullptr);
}

void SceneRenderer::create_framebuffers() {
    const auto& imageViews = m_context.swapchain_image_views();
    m_framebuffers.resize(imageViews.size());

    for (std::size_t i {0U}; i < imageViews.size(); ++i) {
        const VkImageView attachments[] = {imageViews[i], m_albedoImage.view(), m_normalImage.view(), m_linearDepthImage.view(), m_depthImage.view()};

        VkFramebufferCreateInfo framebufferInfo {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = 5U;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = m_context.swapchain_extent().width;
        framebufferInfo.height = m_context.swapchain_extent().height;
        framebufferInfo.layers = 1U;

        if (vkCreateFramebuffer(m_context.device(), &framebufferInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create framebuffer"};
        }
    }
}

void SceneRenderer::destroy_framebuffers() noexcept {
    for (VkFramebuffer framebuffer : m_framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_context.device(), framebuffer, nullptr);
        }
    }
    m_framebuffers.clear();
}

void SceneRenderer::destroy_meshes() noexcept {
    for (GpuMesh& mesh : m_meshes) {
        if (mesh.vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_context.device(), mesh.vertexBuffer, nullptr);
            mesh.vertexBuffer = VK_NULL_HANDLE;
        }
        if (mesh.vertexMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_context.device(), mesh.vertexMemory, nullptr);
            mesh.vertexMemory = VK_NULL_HANDLE;
        }
        if (mesh.indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_context.device(), mesh.indexBuffer, nullptr);
            mesh.indexBuffer = VK_NULL_HANDLE;
        }
        if (mesh.indexMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_context.device(), mesh.indexMemory, nullptr);
            mesh.indexMemory = VK_NULL_HANDLE;
        }
    }
    m_meshes.clear();
}

void SceneRenderer::create_color_resources() {
    const VkPhysicalDevice physicalDevice = m_context.physical_device();
    const VkDevice device = m_context.device();
    const VkExtent2D extent = m_context.swapchain_extent();

    m_albedoImage = vk::ColorImage::create(physicalDevice, device, extent, m_albedoFormat,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    m_normalImage = vk::ColorImage::create(physicalDevice, device, extent, m_normalFormat,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    m_linearDepthImage = vk::ColorImage::create(physicalDevice, device, extent, m_linearDepthFormat,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    if (m_linearDepthImage.image() != VK_NULL_HANDLE) {
        VkCommandPool commandPool {VK_NULL_HANDLE};
        VkCommandBuffer commandBuffer {VK_NULL_HANDLE};
        try {
            VkCommandPoolCreateInfo poolInfo {};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            poolInfo.queueFamilyIndex = m_context.graphics_queue_family_index();

            if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
                throw std::runtime_error {"Failed to create linear depth transition command pool"};
            }

            VkCommandBufferAllocateInfo allocateInfo {};
            allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocateInfo.commandPool = commandPool;
            allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocateInfo.commandBufferCount = 1U;

            if (vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer) != VK_SUCCESS) {
                throw std::runtime_error {"Failed to allocate linear depth transition command buffer"};
            }

            VkCommandBufferBeginInfo beginInfo {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error {"Failed to begin linear depth transition commands"};
            }

            VkImageMemoryBarrier barrier {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = 0U;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.image = m_linearDepthImage.image();
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0U;
            barrier.subresourceRange.levelCount = 1U;
            barrier.subresourceRange.baseArrayLayer = 0U;
            barrier.subresourceRange.layerCount = 1U;

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0U, 0U, nullptr, 0U, nullptr, 1U, &barrier);

            if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
                throw std::runtime_error {"Failed to record linear depth transition commands"};
            }

            VkSubmitInfo submitInfo {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1U;
            submitInfo.pCommandBuffers = &commandBuffer;
            if (vkQueueSubmit(m_context.graphics_queue(), 1U, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
                throw std::runtime_error {"Failed to submit linear depth transition commands"};
            }
            if (vkQueueWaitIdle(m_context.graphics_queue()) != VK_SUCCESS) {
                throw std::runtime_error {"Failed to wait for linear depth transition queue"};
            }
        } catch (...) {
            if (commandPool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(device, commandPool, nullptr);
            }
            throw;
        }
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, commandPool, nullptr);
        }
    }
}

void SceneRenderer::destroy_color_resources() noexcept {
    m_albedoImage = vk::ColorImage {};
    m_normalImage = vk::ColorImage {};
    m_linearDepthImage = vk::ColorImage {};
}

void SceneRenderer::create_material_descriptors() {
    if (m_materialDescriptorLayout != VK_NULL_HANDLE) {
        return;
    }

    std::array<VkDescriptorSetLayoutBinding, 2> bindings {};

    bindings[0].binding = MaterialDescriptorBindings::materialBufferBinding;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1U;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[1].binding = MaterialDescriptorBindings::textureArrayBinding;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = MaterialDescriptorBindings::maxTextureSamplers;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(m_context.device(), &layoutInfo, nullptr, &m_materialDescriptorLayout) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create material descriptor set layout"};
    }

    std::array<VkDescriptorPoolSize, 2> poolSizes {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[0].descriptorCount = 1U;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = MaterialDescriptorBindings::maxTextureSamplers;

    VkDescriptorPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1U;
    poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(m_context.device(), &poolInfo, nullptr, &m_materialDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create material descriptor pool"};
    }

    VkDescriptorSetAllocateInfo allocateInfo {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = m_materialDescriptorPool;
    allocateInfo.descriptorSetCount = 1U;
    allocateInfo.pSetLayouts = &m_materialDescriptorLayout;

    if (vkAllocateDescriptorSets(m_context.device(), &allocateInfo, &m_materialDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to allocate material descriptor set"};
    }
}

void SceneRenderer::destroy_material_descriptors() noexcept {
    if (m_materialDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_context.device(), m_materialDescriptorPool, nullptr);
        m_materialDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_materialDescriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_context.device(), m_materialDescriptorLayout, nullptr);
        m_materialDescriptorLayout = VK_NULL_HANDLE;
    }
    m_materialDescriptorSet = VK_NULL_HANDLE;
    m_materialBuffer = nullptr;
    m_materialTextureInfos.clear();
}

void SceneRenderer::create_light_descriptors() {
    if (m_lightDescriptorLayout != VK_NULL_HANDLE) {
        return;
    }

    VkDescriptorSetLayoutBinding lightBinding {};
    lightBinding.binding = 0U;
    lightBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    lightBinding.descriptorCount = 1U;
    lightBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1U;
    layoutInfo.pBindings = &lightBinding;

    if (vkCreateDescriptorSetLayout(m_context.device(), &layoutInfo, nullptr, &m_lightDescriptorLayout) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create light descriptor set layout"};
    }

    VkDescriptorPoolSize poolSize {};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 1U;

    VkDescriptorPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1U;
    poolInfo.poolSizeCount = 1U;
    poolInfo.pPoolSizes = &poolSize;

    if (vkCreateDescriptorPool(m_context.device(), &poolInfo, nullptr, &m_lightDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create light descriptor pool"};
    }

    VkDescriptorSetAllocateInfo allocateInfo {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = m_lightDescriptorPool;
    allocateInfo.descriptorSetCount = 1U;
    allocateInfo.pSetLayouts = &m_lightDescriptorLayout;

    if (vkAllocateDescriptorSets(m_context.device(), &allocateInfo, &m_lightDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to allocate light descriptor set"};
    }
}

void SceneRenderer::destroy_light_descriptors() noexcept {
    if (m_lightDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_context.device(), m_lightDescriptorPool, nullptr);
        m_lightDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_lightDescriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_context.device(), m_lightDescriptorLayout, nullptr);
        m_lightDescriptorLayout = VK_NULL_HANDLE;
    }
    m_lightDescriptorSet = VK_NULL_HANDLE;
    m_lightBuffer = nullptr;
}

void SceneRenderer::create_depth_resources() {
    m_depthImage = vk::DepthImage::create(
        m_context.physical_device(), m_context.device(), m_context.swapchain_extent(), m_depthFormat);
}

void SceneRenderer::destroy_depth_resources() noexcept {
    m_depthImage = vk::DepthImage {};
}

void SceneRenderer::upload_mesh(const SceneMesh& mesh) {
    const VkDevice device = m_context.device();
    const VkPhysicalDevice physicalDevice = m_context.physical_device();

    if (mesh.material == scene::MaterialId::invalid()) {
        throw std::invalid_argument {"Scene mesh requires a valid material identifier"};
    }

    const std::vector<Vertex> vertices = [&mesh]() {
        std::vector<Vertex> converted;
        converted.reserve(mesh.mesh.vertices.size());
        for (const scene::Vertex& vertex : mesh.mesh.vertices) {
            converted.push_back(Vertex {
                .position = vertex.position,
                .normal = vertex.normal,
                .color = vertex.color,
                .uv = vertex.uv
            });
        }
        return converted;
    }();

    std::vector<std::uint32_t> indices = mesh.mesh.indices;
    if (indices.empty()) {
        indices = build_default_indices(vertices.size());
    }

    GpuMesh gpuMesh {};
    gpuMesh.model = mesh.model;
    gpuMesh.indexCount = static_cast<std::uint32_t>(indices.size());
    gpuMesh.material = mesh.material;

    const VkDeviceSize vertexBufferSize {static_cast<VkDeviceSize>(sizeof(Vertex) * vertices.size())};
    const VkDeviceSize indexBufferSize {static_cast<VkDeviceSize>(sizeof(std::uint32_t) * indices.size())};

    create_buffer(physicalDevice, device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, gpuMesh.vertexBuffer, gpuMesh.vertexMemory);

    create_buffer(physicalDevice, device, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, gpuMesh.indexBuffer, gpuMesh.indexMemory);

    void* vertexData = nullptr;
    if (vkMapMemory(device, gpuMesh.vertexMemory, 0U, vertexBufferSize, 0U, &vertexData) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to map vertex buffer memory"};
    }
    std::memcpy(vertexData, vertices.data(), static_cast<std::size_t>(vertexBufferSize));
    vkUnmapMemory(device, gpuMesh.vertexMemory);

    void* indexData = nullptr;
    if (vkMapMemory(device, gpuMesh.indexMemory, 0U, indexBufferSize, 0U, &indexData) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to map index buffer memory"};
    }
    std::memcpy(indexData, indices.data(), static_cast<std::size_t>(indexBufferSize));
    vkUnmapMemory(device, gpuMesh.indexMemory);

m_meshes.push_back(gpuMesh);
}

void SceneRenderer::create_light_debug_mesh() {
    const VkDevice device = m_context.device();
    const VkPhysicalDevice physicalDevice = m_context.physical_device();

    const std::array<Vertex, 5> vertices {
        Vertex {.position = glm::vec3 {0.0F, 0.0F, 0.0F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
            .color = glm::vec3 {1.0F, 1.0F, 0.0F}, .uv = glm::vec2 {0.0F, 0.0F}},
        Vertex {.position = glm::vec3 {0.05F, 0.05F, -0.2F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
            .color = glm::vec3 {1.0F, 1.0F, 0.0F}, .uv = glm::vec2 {0.0F, 0.0F}},
        Vertex {.position = glm::vec3 {-0.05F, 0.05F, -0.2F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
            .color = glm::vec3 {1.0F, 1.0F, 0.0F}, .uv = glm::vec2 {0.0F, 0.0F}},
        Vertex {.position = glm::vec3 {-0.05F, -0.05F, -0.2F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
            .color = glm::vec3 {1.0F, 1.0F, 0.0F}, .uv = glm::vec2 {0.0F, 0.0F}},
        Vertex {.position = glm::vec3 {0.05F, -0.05F, -0.2F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
            .color = glm::vec3 {1.0F, 1.0F, 0.0F}, .uv = glm::vec2 {0.0F, 0.0F}}
    };
    const std::array<std::uint32_t, 12> indices {
        0U, 1U, 2U,
        0U, 2U, 3U,
        0U, 3U, 4U,
        0U, 4U, 1U
    };

    VkDeviceSize vertexBufferSize {static_cast<VkDeviceSize>(sizeof(Vertex) * vertices.size())};
    VkBufferCreateInfo vertexInfo {};
    vertexInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexInfo.size = vertexBufferSize;
    vertexInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vertexInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &vertexInfo, nullptr, &m_lightDebugMesh.vertexBuffer) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create light debug vertex buffer"};
    }

    VkMemoryRequirements vertexReq {};
    vkGetBufferMemoryRequirements(device, m_lightDebugMesh.vertexBuffer, &vertexReq);

    VkMemoryAllocateInfo vertexAlloc {};
    vertexAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vertexAlloc.allocationSize = vertexReq.size;
    vertexAlloc.memoryTypeIndex = find_memory_type(physicalDevice, vertexReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &vertexAlloc, nullptr, &m_lightDebugMesh.vertexMemory) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to allocate light debug vertex memory"};
    }

    if (vkBindBufferMemory(device, m_lightDebugMesh.vertexBuffer, m_lightDebugMesh.vertexMemory, 0U) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to bind light debug vertex memory"};
    }

    void* vertexData = nullptr;
    if (vkMapMemory(device, m_lightDebugMesh.vertexMemory, 0U, vertexBufferSize, 0U, &vertexData) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to map light debug vertex memory"};
    }
    std::memcpy(vertexData, vertices.data(), static_cast<std::size_t>(vertexBufferSize));
    vkUnmapMemory(device, m_lightDebugMesh.vertexMemory);

    VkDeviceSize indexBufferSize {static_cast<VkDeviceSize>(sizeof(std::uint32_t) * indices.size())};
    VkBufferCreateInfo indexInfo {};
    indexInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    indexInfo.size = indexBufferSize;
    indexInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    indexInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &indexInfo, nullptr, &m_lightDebugMesh.indexBuffer) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create light debug index buffer"};
    }

    VkMemoryRequirements indexReq {};
    vkGetBufferMemoryRequirements(device, m_lightDebugMesh.indexBuffer, &indexReq);

    VkMemoryAllocateInfo indexAlloc {};
    indexAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    indexAlloc.allocationSize = indexReq.size;
    indexAlloc.memoryTypeIndex = find_memory_type(physicalDevice, indexReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &indexAlloc, nullptr, &m_lightDebugMesh.indexMemory) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to allocate light debug index memory"};
    }

    if (vkBindBufferMemory(device, m_lightDebugMesh.indexBuffer, m_lightDebugMesh.indexMemory, 0U) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to bind light debug index memory"};
    }

    void* indexData = nullptr;
    if (vkMapMemory(device, m_lightDebugMesh.indexMemory, 0U, indexBufferSize, 0U, &indexData) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to map light debug index memory"};
    }
    std::memcpy(indexData, indices.data(), static_cast<std::size_t>(indexBufferSize));
    vkUnmapMemory(device, m_lightDebugMesh.indexMemory);

    m_lightDebugMesh.indexCount = static_cast<std::uint32_t>(indices.size());
}

void SceneRenderer::destroy_light_debug_mesh() noexcept {
    const VkDevice device = m_context.device();
    if (m_lightDebugMesh.vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_lightDebugMesh.vertexBuffer, nullptr);
        m_lightDebugMesh.vertexBuffer = VK_NULL_HANDLE;
    }
    if (m_lightDebugMesh.vertexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_lightDebugMesh.vertexMemory, nullptr);
        m_lightDebugMesh.vertexMemory = VK_NULL_HANDLE;
    }
    if (m_lightDebugMesh.indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_lightDebugMesh.indexBuffer, nullptr);
        m_lightDebugMesh.indexBuffer = VK_NULL_HANDLE;
    }
    if (m_lightDebugMesh.indexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_lightDebugMesh.indexMemory, nullptr);
        m_lightDebugMesh.indexMemory = VK_NULL_HANDLE;
    }
    m_lightDebugMesh.indexCount = 0U;
}
} // namespace vulkano::app
