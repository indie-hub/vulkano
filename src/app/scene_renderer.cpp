#include <vulkano/app/scene_renderer.hpp>

#include <vulkano/app/vulkan_context.hpp>
#include <vulkano/app/window.hpp>
#include <vulkano/app/material_buffer.hpp>
#include <vulkano/app/material_texture_cache.hpp>
#include <vulkano/app/light_buffer.hpp>
#include <vulkano/app/shadow_map.hpp>
#include <vulkano/app/shadow_pass.hpp>

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>

#include <vulkano/vk/depth_format.hpp>
#include <vulkano/vk/depth_image.hpp>
#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

#include <glm/vec4.hpp>
#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace {
using vulkano::app::SceneRenderer;
using vulkano::scene::Transform;
namespace scene = vulkano::scene;

struct ShaderPaths final {
    std::filesystem::path vertexPath;
    std::filesystem::path fragmentPath;
};

static_assert(std::is_pointer_v<ImTextureID> || std::is_integral_v<ImTextureID>,
    "ImTextureID must be representable as a pointer or integer type");

template <typename Handle>
ImTextureID to_texture_id(Handle handle) noexcept {
    using HandleType = std::decay_t<Handle>;
    if constexpr (std::is_same_v<HandleType, ImTextureID>) {
        return handle;
    } else if constexpr (std::is_pointer_v<HandleType>) {
        return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(handle));
    } else if constexpr (std::is_integral_v<HandleType>) {
        return static_cast<ImTextureID>(handle);
    } else {
        static_assert(std::is_pointer_v<HandleType> || std::is_integral_v<HandleType>,
            "Unsupported handle type for ImTextureID conversion");
        return ImTextureID_Invalid;
    }
}

scene::Transform compose_local(const scene::Transform& parent, const scene::Transform& local) {
    scene::Transform composed {};
    composed.position = parent.position + local.position;
    composed.rotation = glm::normalize(parent.rotation * local.rotation);
    composed.scale = parent.scale * local.scale;
    return composed;
}

void flatten_nodes(const SceneRenderer::SceneNode& node, const scene::Transform& parentTransform,
    bool parentVisible, std::vector<SceneRenderer::SceneMesh>& out) {
    const scene::Transform worldTransform = compose_local(parentTransform, node.transform);
    const bool nodeVisible = parentVisible && node.visible;

    if (nodeVisible && node.is_mesh() && node.geometry.has_value()) {
        SceneRenderer::SceneMesh mesh {};
        mesh.mesh = node.geometry->mesh;
        mesh.material = node.geometry->material;
        mesh.transform = worldTransform;
        out.push_back(std::move(mesh));
    }

    for (const SceneRenderer::SceneNode& child : node.children) {
        flatten_nodes(child, worldTransform, nodeVisible, out);
    }
}

struct ScenePushConstants final {
    glm::mat4 model {};
    glm::mat4 view {};
    glm::mat4 projection {};
    glm::mat4 lightViewProjection {1.0F};
    glm::uvec4 material {0U, 0U, 0U, 0U};
    glm::vec4 camera {0.0F, 0.0F, 0.0F, 1.0F};
    glm::vec4 shadow {0.002F, 0.0F, 0.0F, 0.0F};
};

struct Vertex final {
    glm::vec3 position {};
    glm::vec3 normal {};
    glm::vec3 color {};
    glm::vec2 uv {};
    glm::vec3 tangent {};
    float bitangentSign {1.0F};
};

[[nodiscard]] VkVertexInputBindingDescription vertex_binding_description() noexcept {
    VkVertexInputBindingDescription binding {};
    binding.binding = 0U;
    binding.stride = static_cast<std::uint32_t>(sizeof(Vertex));
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding;
}

[[nodiscard]] std::array<VkVertexInputAttributeDescription, 6> vertex_attribute_descriptions() noexcept {
    std::array<VkVertexInputAttributeDescription, 6> attributes {};
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
    attributes[4].location = 4U;
    attributes[4].binding = 0U;
    attributes[4].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[4].offset = static_cast<std::uint32_t>(offsetof(Vertex, tangent));
    attributes[5].location = 5U;
    attributes[5].binding = 0U;
    attributes[5].format = VK_FORMAT_R32_SFLOAT;
    attributes[5].offset = static_cast<std::uint32_t>(offsetof(Vertex, bitangentSign));
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
void SceneRenderer::GizmoCache::release() noexcept {
    directional.reset();
    points.clear();
}

void ShadowResources::initialise(
    const VulkanContext& context, VkExtent2D desiredExtent, VkFormat desiredFormat, std::size_t maxSlots) {
    extent = desiredExtent;
    format = desiredFormat;
    if (maxSlots == 0U) {
        maxSlots = 1U;
    }

    const VkDevice device = context.device();

    if (descriptorLayout == VK_NULL_HANDLE) {
        std::array<VkDescriptorSetLayoutBinding, 2> bindings {};

        bindings[0].binding = 0U;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[0].descriptorCount = static_cast<std::uint32_t>(maxSlots);
        bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        bindings[1].binding = 1U;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[1].descriptorCount = 1U;
        bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorLayout) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create shadow descriptor layout"};
        }
    }

    if (descriptorPool == VK_NULL_HANDLE) {
        std::array<VkDescriptorPoolSize, 2> poolSizes {};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[0].descriptorCount = static_cast<std::uint32_t>(maxSlots);
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[1].descriptorCount = 1U;

        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 1U;
        poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create shadow descriptor pool"};
        }
    }

    if (descriptorSet == VK_NULL_HANDLE) {
        VkDescriptorSetAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1U;
        allocInfo.pSetLayouts = &descriptorLayout;

        if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to allocate shadow descriptor set"};
        }
    }

    if (slots.size() != maxSlots) {
        slots.resize(maxSlots);
    }

    activeCount = 0U;
    for (ShadowSlot& slot : slots) {
        slot.id = scene::LightId::invalid();
        slot.active = false;
        slot.dirtyMatrix = true;
        slot.viewProjection = glm::mat4(1.0F);
        slot.priority = 0U;
        slot.layout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (slot.map.render_pass() == VK_NULL_HANDLE) {
            slot.map = ShadowMap {context, extent, format};
            slot.pass = ShadowPass {context, slot.map.render_pass()};
        } else {
            slot.map.recreate(context, extent, format);
            slot.pass.recreate(context, slot.map.render_pass());
        }
    }

    const VkDeviceSize requiredMatrixSize = matrix_payload_size();
    if (matrixBufferSize < requiredMatrixSize || matrixBuffer == VK_NULL_HANDLE) {
        if (matrixBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, matrixBuffer, nullptr);
            matrixBuffer = VK_NULL_HANDLE;
        }
        if (matrixMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, matrixMemory, nullptr);
            matrixMemory = VK_NULL_HANDLE;
        }

        create_buffer(context.physical_device(), device, requiredMatrixSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, matrixBuffer, matrixMemory);
        matrixBufferSize = requiredMatrixSize;
    }

    if (descriptorSet == VK_NULL_HANDLE) {
        VkDescriptorSetAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1U;
        allocInfo.pSetLayouts = &descriptorLayout;

        if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to allocate shadow descriptor set"};
        }
    }

    update_descriptors(context);
    upload_matrices(context);
}

void ShadowResources::release(const VulkanContext& context) noexcept {
    const VkDevice device = context.device();

    if (matrixBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, matrixBuffer, nullptr);
        matrixBuffer = VK_NULL_HANDLE;
    }
    if (matrixMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, matrixMemory, nullptr);
        matrixMemory = VK_NULL_HANDLE;
    }

    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
    if (descriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, descriptorLayout, nullptr);
        descriptorLayout = VK_NULL_HANDLE;
    }
    descriptorSet = VK_NULL_HANDLE;

    activeCount = 0U;
    for (ShadowSlot& slot : slots) {
        slot.map = ShadowMap {};
        slot.pass = ShadowPass {};
        slot.id = scene::LightId::invalid();
        slot.active = false;
        slot.dirtyMatrix = true;
        slot.viewProjection = glm::mat4(1.0F);
        slot.priority = 0U;
        slot.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    slots.clear();
    matrixBufferSize = 0U;
}

void ShadowResources::update_descriptors(const VulkanContext& context) {
    if (descriptorSet == VK_NULL_HANDLE) {
        return;
    }
    std::vector<VkDescriptorImageInfo> infos;
    infos.reserve(slots.size());
    for (const ShadowSlot& slot : slots) {
        infos.push_back(slot.map.descriptor_info());
    }

    VkDescriptorBufferInfo bufferInfo {};
    bufferInfo.buffer = matrixBuffer;
    bufferInfo.offset = 0U;
    bufferInfo.range = matrixBufferSize;

    std::array<VkWriteDescriptorSet, 2> writes {};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = descriptorSet;
    writes[0].dstBinding = 0U;
    writes[0].descriptorCount = static_cast<std::uint32_t>(infos.size());
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = infos.data();

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = descriptorSet;
    writes[1].dstBinding = 1U;
    writes[1].descriptorCount = 1U;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(context.device(), static_cast<std::uint32_t>(writes.size()), writes.data(), 0U, nullptr);
}

bool ShadowResources::empty() const noexcept {
    return slots.empty();
}

std::size_t ShadowResources::slot_count() const noexcept {
    return slots.size();
}

std::uint32_t ShadowResources::active_caster_count() const noexcept {
    return activeCount;
}

VkExtent2D ShadowResources::shadow_extent() const noexcept {
    return extent;
}

VkDeviceSize ShadowResources::matrix_payload_size() const noexcept {
    const VkDeviceSize matrixBytes = static_cast<VkDeviceSize>(sizeof(glm::mat4)) * static_cast<VkDeviceSize>(slots.size());
    return matrixBytes + static_cast<VkDeviceSize>(sizeof(glm::uvec4));
}

void ShadowResources::write_matrix_payload(void* destination) const noexcept {
    auto* matrixPtr = static_cast<glm::mat4*>(destination);
    for (std::size_t index {0U}; index < slots.size(); ++index) {
        matrixPtr[index] = slots[index].viewProjection;
    }
    auto* metaPtr = reinterpret_cast<glm::uvec4*>(matrixPtr + slots.size());
    *metaPtr = glm::uvec4 {activeCount, 0U, 0U, 0U};
}

ShadowSlot& ShadowResources::slot(std::size_t index) {
    return slots.at(index);
}

const ShadowSlot& ShadowResources::slot(std::size_t index) const {
    return slots.at(index);
}

VkDescriptorSetLayout ShadowResources::descriptor_layout() const noexcept {
    return descriptorLayout;
}

VkDescriptorSet ShadowResources::descriptor_set() const noexcept {
    return descriptorSet;
}

void ShadowResources::upload_matrices(const VulkanContext& context) {
    if (matrixBuffer == VK_NULL_HANDLE) {
        return;
    }

    void* mapped = nullptr;
    if (vkMapMemory(context.device(), matrixMemory, 0U, matrixBufferSize, 0U, &mapped) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to map shadow matrix buffer"};
    }

    write_matrix_payload(mapped);

    vkUnmapMemory(context.device(), matrixMemory);
}
SceneRenderer::SceneRenderer(const VulkanContext& context, const Window& window, VkDescriptorSetLayout ssaoLayout)
    : m_context {context}
    , m_descriptorLayout {ssaoLayout} {
    static_cast<void>(window);
    m_depthFormat = vk::DepthFormatResolver::select_depth_format(m_context.physical_device());
    m_viewportExtent = m_context.swapchain_extent();
    create_scene_render_pass();
    create_present_render_pass();
    create_material_descriptors();
    create_light_descriptors();
    create_shadow_resources();
    create_pipeline_layout();
    create_graphics_pipeline();
    create_color_resources();
    create_depth_resources();
    create_scene_framebuffer();
    create_present_framebuffers();
    create_light_debug_mesh();

}

SceneRenderer::~SceneRenderer() noexcept {
    destroy_meshes();
    destroy_present_framebuffers();
    destroy_scene_framebuffer();
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
    m_gizmoCache.release();
    destroy_light_debug_mesh();
    destroy_shadow_resources();
    if (m_sceneRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_context.device(), m_sceneRenderPass, nullptr);
        m_sceneRenderPass = VK_NULL_HANDLE;
    }
    if (m_presentRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_context.device(), m_presentRenderPass, nullptr);
        m_presentRenderPass = VK_NULL_HANDLE;
    }
    destroy_viewport_descriptor();
    if (m_viewportTexture != nullptr) {
        if (ImGui::GetCurrentContext() != nullptr) {
            ImGui::UnregisterUserTexture(m_viewportTexture);
        }
        IM_DELETE(m_viewportTexture);
        m_viewportTexture = nullptr;
    }
}

void SceneRenderer::set_scene(const std::vector<SceneMesh>& meshes) {
    destroy_meshes();
    m_sceneBoundsValid = false;
    glm::vec3 minBounds {std::numeric_limits<float>::max()};
    glm::vec3 maxBounds {std::numeric_limits<float>::lowest()};
    m_meshes.reserve(meshes.size());
    for (const SceneMesh& mesh : meshes) {
        upload_mesh(mesh);
        const glm::mat4 modelMatrix = mesh.transform.matrix();
        for (const scene::Vertex& vertex : mesh.mesh.vertices) {
            const glm::vec4 worldPosition = modelMatrix * glm::vec4(vertex.position, 1.0F);
            minBounds = glm::min(minBounds, glm::vec3(worldPosition));
            maxBounds = glm::max(maxBounds, glm::vec3(worldPosition));
        }
    }
    if (!meshes.empty()) {
        m_sceneMin = minBounds;
        m_sceneMax = maxBounds;
        m_sceneBoundsValid = true;
    }
}

void SceneRenderer::set_scene_graph(const SceneNode& root) {
    std::vector<SceneMesh> flattened;
    flattened.reserve(64U);
    flatten_nodes(root, scene::Transform::identity(), root.visible, flattened);
    set_scene(flattened);
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

void SceneRenderer::set_light_resources(LightBuffer& buffer, const scene::LightRegistry& registry) {
    destroy_point_light_debug_meshes();
    std::vector<DebugMesh> pointMeshes;
    std::vector<LightGizmoHandle> pointHandles;

    struct DirectionalInfo {
        scene::LightId id;
        const scene::Light* light;
    };

    std::vector<DirectionalInfo> directionalLights;
    std::vector<DirectionalInfo> shadowCasters;
    directionalLights.reserve(registry.size());
    shadowCasters.reserve(registry.size());

    for (std::size_t index {0U}; index < registry.size(); ++index) {
        const scene::LightId id {static_cast<std::uint32_t>(index)};
        const scene::Light& candidate = registry.light(id);
        if (candidate.type != scene::LightType::Directional) {
            continue;
        }
        directionalLights.push_back(DirectionalInfo {id, &candidate});
        if (candidate.castsShadow) {
            shadowCasters.push_back(DirectionalInfo {id, &candidate});
        }
    }

    const std::size_t maxSlots = m_shadowResources.slot_count();
    const std::size_t activeSlots = std::min(maxSlots, shadowCasters.size());
    std::unordered_map<std::uint32_t, std::uint32_t> shadowIndexMap;
    shadowIndexMap.reserve(activeSlots);
    for (std::size_t slotIndex {0U}; slotIndex < maxSlots; ++slotIndex) {
        ShadowSlot& slot = m_shadowResources.slot(slotIndex);
        if (slotIndex < activeSlots) {
            const DirectionalInfo& info = shadowCasters[slotIndex];
            const bool idChanged = slot.id != info.id;
            slot.id = info.id;
            slot.active = true;
            slot.priority = static_cast<std::uint32_t>(info.id.value);
            slot.dirtyMatrix = static_cast<bool>(slot.dirtyMatrix) || idChanged;
            slot.viewProjection = compute_light_view_projection(*info.light);
            slot.dirtyMatrix = false;
            if (idChanged) {
                slot.layout = VK_IMAGE_LAYOUT_UNDEFINED;
            }
            shadowIndexMap.emplace(info.id.value, static_cast<std::uint32_t>(slotIndex));
        } else {
            slot.id = scene::LightId::invalid();
            slot.active = false;
            slot.priority = 0U;
            slot.dirtyMatrix = true;
            slot.viewProjection = glm::mat4(1.0F);
            slot.layout = VK_IMAGE_LAYOUT_UNDEFINED;
        }
    }
    m_shadowResources.activeCount = static_cast<std::uint32_t>(activeSlots);
    m_shadowResources.upload_matrices(m_context);

    buffer.update(registry, shadowIndexMap);

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

    const DirectionalInfo* primaryInfo = nullptr;
    if (!shadowCasters.empty()) {
        primaryInfo = &shadowCasters.front();
    } else if (!directionalLights.empty()) {
        primaryInfo = &directionalLights.front();
    }

    if (primaryInfo != nullptr) {
        const glm::vec3 dir = primaryInfo->light->direction;
        m_lightDirection = glm::length(dir) > 0.0F ? glm::normalize(dir) : glm::vec3 {0.0F, -1.0F, 0.0F};
        m_lightColor = primaryInfo->light->color;
        m_lightIntensity = primaryInfo->light->intensity;
        m_primaryLightCastsShadow = primaryInfo->light->castsShadow && activeSlots > 0U;

        if (maxSlots > 0U) {
            ShadowSlot& primarySlot = m_shadowResources.slot(0);
            if (primarySlot.active) {
                primarySlot.dirtyMatrix = true;
            }
        }

        LightGizmoHandle handle {};
        handle.id = primaryInfo->id;
        handle.mesh = &m_lightDebugMesh;
        handle.type = scene::LightType::Directional;
        handle.transform = glm::mat4(1.0F);
        handle.dirty = true;
        if (m_gizmoCache.directional) {
            LightGizmoHandle& existing = *m_gizmoCache.directional;
            existing = handle;
        } else {
            m_gizmoCache.directional = handle;
        }
    } else {
        m_lightDirection = glm::vec3 {0.0F, -1.0F, 0.0F};
        m_lightColor = glm::vec3 {1.0F, 1.0F, 1.0F};
        m_lightIntensity = 1.0F;
        m_primaryLightCastsShadow = false;
        if (maxSlots > 0U) {
            ShadowSlot& primarySlot = m_shadowResources.slot(0);
            primarySlot.id = scene::LightId::invalid();
            primarySlot.active = false;
            primarySlot.dirtyMatrix = true;
        }
        m_gizmoCache.directional.reset();
    }

    if (m_lightDebugMesh.vertexMemory != VK_NULL_HANDLE) {
        const glm::vec3 arrowColor = primaryInfo != nullptr ? primaryInfo->light->color : glm::vec3 {1.0F, 1.0F, 0.0F};
        std::array<Vertex, 5> vertices = {
            Vertex {.position = glm::vec3 {0.0F, 0.5F, 0.0F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
                .color = arrowColor, .uv = glm::vec2 {0.0F, 0.0F}, .tangent = glm::vec3 {1.0F, 0.0F, 0.0F}},
            Vertex {.position = glm::vec3 {0.15F, 0.35F, -0.6F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
                .color = arrowColor, .uv = glm::vec2 {0.0F, 0.0F}, .tangent = glm::vec3 {1.0F, 0.0F, 0.0F}},
            Vertex {.position = glm::vec3 {-0.15F, 0.35F, -0.6F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
                .color = arrowColor, .uv = glm::vec2 {0.0F, 0.0F}, .tangent = glm::vec3 {1.0F, 0.0F, 0.0F}},
            Vertex {.position = glm::vec3 {-0.15F, 0.15F, -0.6F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
                .color = arrowColor, .uv = glm::vec2 {0.0F, 0.0F}, .tangent = glm::vec3 {1.0F, 0.0F, 0.0F}},
            Vertex {.position = glm::vec3 {0.15F, 0.15F, -0.6F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
                .color = arrowColor, .uv = glm::vec2 {0.0F, 0.0F}, .tangent = glm::vec3 {1.0F, 0.0F, 0.0F}}
        };
        VkDeviceSize size = static_cast<VkDeviceSize>(sizeof(Vertex) * vertices.size());
        void* mapped = nullptr;
        if (vkMapMemory(m_context.device(), m_lightDebugMesh.vertexMemory, 0U, size, 0U, &mapped) == VK_SUCCESS) {
            std::memcpy(mapped, vertices.data(), static_cast<std::size_t>(size));
            vkUnmapMemory(m_context.device(), m_lightDebugMesh.vertexMemory);
        }
    }

    for (std::size_t index {0U}; index < registry.size(); ++index) {
        const scene::Light& light = registry.light(scene::LightId {static_cast<std::uint32_t>(index)});
        if (light.type != scene::LightType::Point) {
            continue;
        }

        DebugMesh mesh {};
        const std::array<Vertex, 6> vertices {
            Vertex {.position = glm::vec3 {0.0F, 0.0F, 0.0F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
                .color = light.color, .uv = glm::vec2 {0.0F, 0.0F}, .tangent = glm::vec3 {1.0F, 0.0F, 0.0F}},
            Vertex {.position = glm::vec3 {0.0F, 0.2F, 0.0F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
                .color = light.color, .uv = glm::vec2 {0.0F, 0.0F}, .tangent = glm::vec3 {1.0F, 0.0F, 0.0F}},
            Vertex {.position = glm::vec3 {0.2F, 0.0F, 0.0F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
                .color = light.color, .uv = glm::vec2 {0.0F, 0.0F}, .tangent = glm::vec3 {1.0F, 0.0F, 0.0F}},
            Vertex {.position = glm::vec3 {0.0F, -0.2F, 0.0F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
                .color = light.color, .uv = glm::vec2 {0.0F, 0.0F}, .tangent = glm::vec3 {1.0F, 0.0F, 0.0F}},
            Vertex {.position = glm::vec3 {-0.2F, 0.0F, 0.0F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
                .color = light.color, .uv = glm::vec2 {0.0F, 0.0F}, .tangent = glm::vec3 {1.0F, 0.0F, 0.0F}},
            Vertex {.position = glm::vec3 {0.0F, 0.0F, 0.2F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
                .color = light.color, .uv = glm::vec2 {0.0F, 0.0F}, .tangent = glm::vec3 {1.0F, 0.0F, 0.0F}}
        };
        const std::array<std::uint32_t, 12> indices {0U, 1U, 2U, 0U, 2U, 3U, 0U, 3U, 4U, 0U, 4U, 1U};

        const VkDeviceSize vertexSize = static_cast<VkDeviceSize>(sizeof(Vertex) * vertices.size());
        create_buffer(m_context.physical_device(), m_context.device(), vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mesh.vertexBuffer,
            mesh.vertexMemory);
        void* mapped = nullptr;
        if (vkMapMemory(m_context.device(), mesh.vertexMemory, 0U, vertexSize, 0U, &mapped) == VK_SUCCESS) {
            std::memcpy(mapped, vertices.data(), static_cast<std::size_t>(vertexSize));
            vkUnmapMemory(m_context.device(), mesh.vertexMemory);
        }

        const VkDeviceSize indexSize = static_cast<VkDeviceSize>(sizeof(std::uint32_t) * indices.size());
        create_buffer(m_context.physical_device(), m_context.device(), indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mesh.indexBuffer,
            mesh.indexMemory);
        if (vkMapMemory(m_context.device(), mesh.indexMemory, 0U, indexSize, 0U, &mapped) == VK_SUCCESS) {
            std::memcpy(mapped, indices.data(), static_cast<std::size_t>(indexSize));
            vkUnmapMemory(m_context.device(), mesh.indexMemory);
        }

        mesh.indexCount = static_cast<std::uint32_t>(indices.size());
        mesh.model = glm::translate(glm::mat4(1.0F), light.position) * glm::scale(glm::mat4(1.0F), glm::vec3(0.25F));
        LightGizmoHandle handle {};
        handle.id = scene::LightId {static_cast<std::uint32_t>(index)};
        handle.mesh = nullptr;
        handle.type = scene::LightType::Point;
        handle.transform = mesh.model;
        handle.dirty = false;
        pointHandles.push_back(handle);
        pointMeshes.push_back(std::move(mesh));
    }

    m_pointLightDebugMeshes = std::move(pointMeshes);
    m_gizmoCache.points.clear();
    m_gizmoCache.points.reserve(m_pointLightDebugMeshes.size());
    for (std::size_t idx {0U}; idx < m_pointLightDebugMeshes.size(); ++idx) {
        LightGizmoHandle handle = pointHandles[idx];
        handle.mesh = &m_pointLightDebugMeshes[idx];
        m_gizmoCache.points.push_back(handle);
    }

    m_lightBuffer = &buffer;
}

void SceneRenderer::set_show_light_debug(bool enabled) noexcept {
    m_showLightDebug = enabled;
}

VkPipeline SceneRenderer::pipeline() const noexcept {
    return m_pipeline;
}

VkPipelineLayout SceneRenderer::pipeline_layout() const noexcept {
    return m_pipelineLayout;
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

float SceneRenderer::shadow_bias() const noexcept {
    return m_shadowBias;
}

float SceneRenderer::shadow_pcf_radius() const noexcept {
    return m_shadowPcfRadius;
}

bool SceneRenderer::shadows_enabled() const noexcept {
    return m_shadowsEnabled;
}

void SceneRenderer::set_shadow_bias(float bias) noexcept {
    m_shadowBias = glm::clamp(bias, 0.0F, 0.05F);
}

void SceneRenderer::set_shadow_pcf_radius(float radius) noexcept {
    m_shadowPcfRadius = glm::clamp(radius, 0.0F, 4.0F);
}

void SceneRenderer::set_shadows_enabled(bool enabled) noexcept {
    m_shadowsEnabled = enabled;
}

bool SceneRenderer::shadow_debug_enabled() const noexcept {
    return m_shadowDebug;
}

std::uint32_t SceneRenderer::shadow_slot_capacity() const noexcept {
    return static_cast<std::uint32_t>(m_shadowResources.slot_count());
}

std::uint32_t SceneRenderer::shadow_active_caster_count() const noexcept {
    return m_shadowResources.active_caster_count();
}

VkExtent2D SceneRenderer::shadow_map_extent() const noexcept {
    return m_shadowResources.shadow_extent();
}

std::optional<std::uint32_t> SceneRenderer::shadow_slot_for_light(scene::LightId id) const noexcept {
    const std::size_t slotCount = m_shadowResources.slot_count();
    for (std::size_t index {0U}; index < slotCount; ++index) {
        const ShadowSlot& slot = m_shadowResources.slot(index);
        if (slot.active && slot.id == id) {
            return static_cast<std::uint32_t>(index);
        }
    }
    return std::nullopt;
}

void SceneRenderer::set_shadow_debug_enabled(bool enabled) noexcept {
    m_shadowDebug = enabled;
}

bool SceneRenderer::set_viewport_extent(VkExtent2D extent) {
    if (extent.width == 0U || extent.height == 0U) {
        return false;
    }
    if (extent.width == m_viewportExtent.width && extent.height == m_viewportExtent.height) {
        return false;
    }

    m_viewportExtent = extent;
    recreate_viewport_resources();
    return true;
}

VkExtent2D SceneRenderer::viewport_extent() const noexcept {
    return m_viewportExtent;
}

VkImageView SceneRenderer::scene_color_image_view() const noexcept {
    return m_sceneColorImage.view();
}

VkFormat SceneRenderer::scene_color_format() const noexcept {
    return m_sceneColorImage.format();
}

VkRenderPass SceneRenderer::scene_render_pass() const noexcept {
    return m_sceneRenderPass;
}

VkRenderPass SceneRenderer::present_render_pass() const noexcept {
    return m_presentRenderPass;
}

VkFramebuffer SceneRenderer::scene_framebuffer() const noexcept {
    return m_sceneFramebuffer;
}

const std::vector<VkFramebuffer>& SceneRenderer::present_framebuffers() const noexcept {
    return m_presentFramebuffers;
}

VkDescriptorSet SceneRenderer::viewport_descriptor() noexcept {
    ensure_viewport_descriptor();
    return m_viewportDescriptor;
}

ImTextureID SceneRenderer::viewport_texture_id() noexcept {
    ensure_viewport_descriptor();
    if (m_viewportTexture != nullptr) {
        return m_viewportTexture->GetTexID();
    }
    if (m_viewportDescriptor == VK_NULL_HANDLE) {
        return ImTextureID_Invalid;
    }
    return to_texture_id(m_viewportDescriptor);
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

    const auto source_stage_for_layout = [](VkImageLayout layout) noexcept -> VkPipelineStageFlags {
        switch (layout) {
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case VK_IMAGE_LAYOUT_UNDEFINED:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        default:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }
    };

    const auto source_access_for_layout = [](VkImageLayout layout) noexcept -> VkAccessFlags {
        switch (layout) {
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
            return VK_ACCESS_SHADER_READ_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_ACCESS_SHADER_READ_BIT;
        default:
            return VkAccessFlags {0U};
        }
    };

    const auto transition_to_color_attachment = [&](const vk::ColorImage& image, VkImageLayout& layout) {
        if (image.image() == VK_NULL_HANDLE) {
            return;
        }
        if (layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
            return;
        }

        VkImageMemoryBarrier barrier {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = source_access_for_layout(layout);
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = layout;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image.image();
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0U;
        barrier.subresourceRange.levelCount = 1U;
        barrier.subresourceRange.baseArrayLayer = 0U;
        barrier.subresourceRange.layerCount = 1U;

        vkCmdPipelineBarrier(commandBuffer, source_stage_for_layout(layout), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0U, 0U, nullptr, 0U, nullptr, 1U, &barrier);

        layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    };

    transition_to_color_attachment(m_sceneColorImage, m_sceneColorLayout);
    transition_to_color_attachment(m_albedoImage, m_albedoLayout);
    transition_to_color_attachment(m_normalImage, m_normalLayout);
    transition_to_color_attachment(m_linearDepthImage, m_linearDepthLayout);

    for (std::uint32_t slotIndex {0U}; slotIndex < m_shadowResources.slot_count(); ++slotIndex) {
        ShadowSlot& slot = m_shadowResources.slot(slotIndex);
        if (slot.active || slot.map.image() == VK_NULL_HANDLE) {
            continue;
        }
        if (slot.layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {
            continue;
        }

        VkImageMemoryBarrier barrier {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = source_access_for_layout(slot.layout);
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = slot.layout;
        barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = slot.map.image();
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.subresourceRange.baseMipLevel = 0U;
        barrier.subresourceRange.levelCount = 1U;
        barrier.subresourceRange.baseArrayLayer = 0U;
        barrier.subresourceRange.layerCount = 1U;

        vkCmdPipelineBarrier(commandBuffer, source_stage_for_layout(slot.layout),
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0U, 0U, nullptr, 0U, nullptr, 1U, &barrier);

        slot.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    }

    const bool hasShadowPass = m_shadowsEnabled && m_shadowResources.activeCount > 0U;
    if (hasShadowPass) {

        for (std::uint32_t slotIndex {0U}; slotIndex < m_shadowResources.activeCount; ++slotIndex) {
            ShadowSlot& slot = m_shadowResources.slot(slotIndex);
            if (!slot.active || slot.map.image() == VK_NULL_HANDLE) {
                continue;
            }

            if (slot.layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
                continue;
            }

            VkImageMemoryBarrier barrier {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = source_access_for_layout(slot.layout);
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            barrier.oldLayout = slot.layout;
            barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = slot.map.image();
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            barrier.subresourceRange.baseMipLevel = 0U;
            barrier.subresourceRange.levelCount = 1U;
            barrier.subresourceRange.baseArrayLayer = 0U;
            barrier.subresourceRange.layerCount = 1U;

            vkCmdPipelineBarrier(commandBuffer, source_stage_for_layout(slot.layout),
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0U, 0U, nullptr,
                0U, nullptr, 1U, &barrier);

            slot.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        record_shadow_pass(commandBuffer);

        for (std::uint32_t slotIndex {0U}; slotIndex < m_shadowResources.activeCount; ++slotIndex) {
            ShadowSlot& slot = m_shadowResources.slot(slotIndex);
            if (!slot.active || slot.map.image() == VK_NULL_HANDLE) {
                continue;
            }

            VkImageMemoryBarrier barrier {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = slot.layout;
            barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = slot.map.image();
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            barrier.subresourceRange.baseMipLevel = 0U;
            barrier.subresourceRange.levelCount = 1U;
            barrier.subresourceRange.baseArrayLayer = 0U;
            barrier.subresourceRange.layerCount = 1U;

            const VkPipelineStageFlags sourceStage = (slot.layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
                : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

            vkCmdPipelineBarrier(commandBuffer, sourceStage, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0U, 0U, nullptr,
                0U, nullptr, 1U, &barrier);

            slot.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        }
    }

    const glm::mat4 lightMatrix = (hasShadowPass && m_shadowResources.slot_count() > 0U)
        ? m_shadowResources.slot(0).viewProjection
        : glm::mat4(1.0F);
    const float shadowEnabled = hasShadowPass ? 1.0F : 0.0F;
    const glm::vec4 shadowParams {m_shadowBias, m_shadowPcfRadius, shadowEnabled, m_shadowDebug ? 1.0F : 0.0F};

    VkClearValue albedoClear {};
    albedoClear.color = {{0.0F, 0.0F, 0.0F, 1.0F}};
    VkClearValue normalClear {};
    normalClear.color = {{0.5F, 0.5F, 1.0F, 1.0F}};
    VkClearValue linearDepthClear {};
    linearDepthClear.color = {{0.0F, 0.0F, 0.0F, 0.0F}};
    VkClearValue depthClear {};
    depthClear.depthStencil = {1.0F, 0U};
    const std::array<VkClearValue, 5> sceneClearValues {VkClearValue {.color = {{0.0F, 0.0F, 0.0F, 1.0F}}}, albedoClear,
        normalClear, linearDepthClear, depthClear};

    VkRenderPassBeginInfo scenePassInfo {};
    scenePassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    scenePassInfo.renderPass = m_sceneRenderPass;
    scenePassInfo.framebuffer = m_sceneFramebuffer;
    scenePassInfo.renderArea.offset = {0, 0};
    scenePassInfo.renderArea.extent = m_viewportExtent;
    scenePassInfo.clearValueCount = static_cast<std::uint32_t>(sceneClearValues.size());
    scenePassInfo.pClearValues = sceneClearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &scenePassInfo, VK_SUBPASS_CONTENTS_INLINE);
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
    if (!m_shadowResources.empty()) {
        const VkDescriptorSet shadowSet = m_shadowResources.descriptor_set();
        if (shadowSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 3U, 1U, &shadowSet,
                0U, nullptr);
        }
    }

    const VkViewport viewport {
        .x = 0.0F,
        .y = 0.0F,
        .width = static_cast<float>(m_viewportExtent.width),
        .height = static_cast<float>(m_viewportExtent.height),
        .minDepth = 0.0F,
        .maxDepth = 1.0F};
    vkCmdSetViewport(commandBuffer, 0U, 1U, &viewport);

    const VkRect2D scissor {
        .offset = {0, 0},
        .extent = m_viewportExtent};
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
            .lightViewProjection = lightMatrix,
            .material = glm::uvec4 {mesh.material.value, 0U, 0U, 0U},
            .camera = glm::vec4 {cameraPosition, 1.0F},
            .shadow = shadowParams
        };
        vkCmdPushConstants(commandBuffer, m_pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0U,
            static_cast<std::uint32_t>(sizeof(ScenePushConstants)), &pushConstants);

        vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1U, 0U, 0, 0U);
    }

    if (m_showLightDebug && m_lightBuffer != nullptr) {
        if (m_gizmoCache.directional) {
            const LightGizmoHandle& handle = *m_gizmoCache.directional;
            const DebugMesh* debugMesh = handle.mesh;
            if (debugMesh != nullptr && debugMesh->indexCount > 0U) {
            const glm::vec3 dir = glm::length(m_lightDirection) > 0.0F ? glm::normalize(m_lightDirection)
                : glm::vec3 {0.0F, -1.0F, 0.0F};
            const float length = glm::max(m_lightIntensity, 0.1F);

            glm::vec3 forward = -dir;
            glm::vec3 upReference = glm::abs(forward.y) > 0.99F ? glm::vec3 {0.0F, 0.0F, 1.0F}
                                                                  : glm::vec3 {0.0F, 1.0F, 0.0F};
            glm::vec3 right = glm::normalize(glm::cross(upReference, forward));
            glm::vec3 up = glm::normalize(glm::cross(forward, right));

            glm::mat4 debugModel {1.0F};
            debugModel[0] = glm::vec4(right * 0.5F, 0.0F);
            debugModel[1] = glm::vec4(up * 0.5F, 0.0F);
            debugModel[2] = glm::vec4(forward * length, 0.0F);
            debugModel[3] = glm::vec4(0.0F, 0.5F, 0.0F, 1.0F);
            const ScenePushConstants debugConstants {
                .model = debugModel,
                .view = view,
                .projection = projection,
                .lightViewProjection = lightMatrix,
                .material = glm::uvec4 {0U, 0U, 0U, 0U},
                .camera = glm::vec4 {cameraPosition, 1.0F},
                .shadow = shadowParams
            };

            const VkBuffer lightBuffers[] = {debugMesh->vertexBuffer};
            const VkDeviceSize lightOffsets[] = {0U};
            vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, lightBuffers, lightOffsets);
            vkCmdBindIndexBuffer(commandBuffer, debugMesh->indexBuffer, 0U, VK_INDEX_TYPE_UINT32);
            vkCmdPushConstants(commandBuffer, m_pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0U,
                static_cast<std::uint32_t>(sizeof(ScenePushConstants)), &debugConstants);
            vkCmdDrawIndexed(commandBuffer, debugMesh->indexCount, 1U, 0U, 0, 0U);
            }
        }

        for (const LightGizmoHandle& pointHandle : m_gizmoCache.points) {
            const DebugMesh* mesh = pointHandle.mesh;
            if (mesh == nullptr || mesh->indexCount == 0U) {
                continue;
            }

            const ScenePushConstants pointConstants {
                .model = pointHandle.transform,
                .view = view,
                .projection = projection,
                .lightViewProjection = lightMatrix,
                .material = glm::uvec4 {0U, 0U, 0U, 0U},
                .camera = glm::vec4 {cameraPosition, 1.0F},
                .shadow = shadowParams
            };

            const VkBuffer pointBuffers[] = {mesh->vertexBuffer};
            const VkDeviceSize pointOffsets[] = {0U};
            vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, pointBuffers, pointOffsets);
            vkCmdBindIndexBuffer(commandBuffer, mesh->indexBuffer, 0U, VK_INDEX_TYPE_UINT32);
            vkCmdPushConstants(commandBuffer, m_pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0U,
                static_cast<std::uint32_t>(sizeof(ScenePushConstants)), &pointConstants);
            vkCmdDrawIndexed(commandBuffer, mesh->indexCount, 1U, 0U, 0, 0U);
        }
    }

    vkCmdEndRenderPass(commandBuffer);

    std::array<VkImageMemoryBarrier, 4> colorReadBarriers {};
    std::uint32_t colorBarrierCount {0U};
    const auto appendColorBarrier = [&](VkImage image, VkImageLayout& layout) noexcept {
        if (image == VK_NULL_HANDLE) {
            return;
        }
        constexpr VkImageLayout targetLayout {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkImageMemoryBarrier barrier {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = targetLayout;
        barrier.newLayout = targetLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0U;
        barrier.subresourceRange.levelCount = 1U;
        barrier.subresourceRange.baseArrayLayer = 0U;
        barrier.subresourceRange.layerCount = 1U;
        colorReadBarriers[colorBarrierCount++] = barrier;
        layout = targetLayout;
    };

    appendColorBarrier(m_sceneColorImage.image(), m_sceneColorLayout);
    appendColorBarrier(m_albedoImage.image(), m_albedoLayout);
    appendColorBarrier(m_normalImage.image(), m_normalLayout);
    appendColorBarrier(m_linearDepthImage.image(), m_linearDepthLayout);

    if (colorBarrierCount > 0U) {
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0U, 0U, nullptr, 0U, nullptr, colorBarrierCount,
            colorReadBarriers.data());
    }

    VkClearValue presentClear {};
    presentClear.color = {{0.05F, 0.05F, 0.05F, 1.0F}};

    VkRenderPassBeginInfo presentPassInfo {};
    presentPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    presentPassInfo.renderPass = m_presentRenderPass;
    presentPassInfo.framebuffer = m_presentFramebuffers[imageIndex];
    presentPassInfo.renderArea.offset = {0, 0};
    presentPassInfo.renderArea.extent = m_context.swapchain_extent();
    presentPassInfo.clearValueCount = 1U;
    presentPassInfo.pClearValues = &presentClear;

    vkCmdBeginRenderPass(commandBuffer, &presentPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    if (overlayRecorder) {
        overlayRecorder(commandBuffer);
    }
    vkCmdEndRenderPass(commandBuffer);
}

void SceneRenderer::create_scene_render_pass() {
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
    linearDepthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
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

    VkAttachmentDescription sceneColorAttachment {};
    sceneColorAttachment.format = m_context.swapchain_image_format();
    sceneColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    sceneColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    sceneColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    sceneColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    sceneColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    sceneColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    sceneColorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

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
        sceneColorAttachment,
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

    if (vkCreateRenderPass(m_context.device(), &renderPassInfo, nullptr, &m_sceneRenderPass) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create render pass"};
    }
}

void SceneRenderer::create_present_render_pass() {
    VkAttachmentDescription colorAttachment {};
    colorAttachment.format = m_context.swapchain_image_format();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef {};
    colorRef.attachment = 0U;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1U;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dependency {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0U;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0U;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1U;
    info.pAttachments = &colorAttachment;
    info.subpassCount = 1U;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1U;
    info.pDependencies = &dependency;

    if (vkCreateRenderPass(m_context.device(), &info, nullptr, &m_presentRenderPass) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create present render pass"};
    }
}

void SceneRenderer::create_pipeline_layout() {
    VkPushConstantRange pushConstantRange {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0U;
    pushConstantRange.size = static_cast<std::uint32_t>(sizeof(ScenePushConstants));

    VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    std::array<VkDescriptorSetLayout, 4> layouts {};
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
    const VkDescriptorSetLayout shadowLayout = m_shadowResources.descriptor_layout();
    if (shadowLayout == VK_NULL_HANDLE) {
        throw std::logic_error {"Shadow resources descriptor layout not initialised"};
    }
    layouts[layoutCount++] = m_descriptorLayout;
    layouts[layoutCount++] = m_materialDescriptorLayout;
    layouts[layoutCount++] = m_lightDescriptorLayout;
    layouts[layoutCount++] = shadowLayout;
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
    pipelineInfo.renderPass = m_sceneRenderPass;
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

void SceneRenderer::create_scene_framebuffer() {
    destroy_scene_framebuffer();

    const VkImageView attachments[] = {m_sceneColorImage.view(), m_albedoImage.view(), m_normalImage.view(),
        m_linearDepthImage.view(), m_depthImage.view()};

    VkFramebufferCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.renderPass = m_sceneRenderPass;
    info.attachmentCount = 5U;
    info.pAttachments = attachments;
    info.width = m_viewportExtent.width;
    info.height = m_viewportExtent.height;
    info.layers = 1U;

    if (vkCreateFramebuffer(m_context.device(), &info, nullptr, &m_sceneFramebuffer) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create scene framebuffer"};
    }
}

void SceneRenderer::destroy_scene_framebuffer() noexcept {
    if (m_sceneFramebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(m_context.device(), m_sceneFramebuffer, nullptr);
        m_sceneFramebuffer = VK_NULL_HANDLE;
    }
}

void SceneRenderer::create_present_framebuffers() {
    destroy_present_framebuffers();

    const auto& imageViews = m_context.swapchain_image_views();
    m_presentFramebuffers.resize(imageViews.size());

    for (std::size_t i {0U}; i < imageViews.size(); ++i) {
        VkFramebufferCreateInfo info {};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = m_presentRenderPass;
        info.attachmentCount = 1U;
        info.pAttachments = &imageViews[i];
        info.width = m_context.swapchain_extent().width;
        info.height = m_context.swapchain_extent().height;
        info.layers = 1U;

        if (vkCreateFramebuffer(m_context.device(), &info, nullptr, &m_presentFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create present framebuffer"};
        }
    }
}

void SceneRenderer::destroy_present_framebuffers() noexcept {
    for (VkFramebuffer framebuffer : m_presentFramebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_context.device(), framebuffer, nullptr);
        }
    }
    m_presentFramebuffers.clear();
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
    m_sceneBoundsValid = false;
}

void SceneRenderer::create_color_resources() {
    const VkPhysicalDevice physicalDevice = m_context.physical_device();
    const VkDevice device = m_context.device();
    const VkExtent2D extent = m_viewportExtent;

    m_sceneColorImage = vk::ColorImage::create(physicalDevice, device, extent, m_context.swapchain_image_format(),
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    m_albedoImage = vk::ColorImage::create(physicalDevice, device, extent, m_albedoFormat,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    m_normalImage = vk::ColorImage::create(physicalDevice, device, extent, m_normalFormat,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    m_linearDepthImage = vk::ColorImage::create(physicalDevice, device, extent, m_linearDepthFormat,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    m_sceneColorLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_albedoLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_normalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_linearDepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;

}

void SceneRenderer::destroy_color_resources() noexcept {
    m_sceneColorImage = vk::ColorImage {};
    m_albedoImage = vk::ColorImage {};
    m_normalImage = vk::ColorImage {};
    m_linearDepthImage = vk::ColorImage {};
    m_sceneColorLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_albedoLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_normalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_linearDepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_viewportImageView = VK_NULL_HANDLE;
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
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
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
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
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
        m_context.physical_device(), m_context.device(), m_viewportExtent, m_depthFormat);
}

void SceneRenderer::destroy_depth_resources() noexcept {
    m_depthImage = vk::DepthImage {};
}

void SceneRenderer::create_shadow_resources() {
    constexpr std::size_t maxShadowCasters {3U};
    m_shadowResources.initialise(m_context, m_shadowExtent, m_shadowFormat, maxShadowCasters);
}

void SceneRenderer::destroy_shadow_resources() noexcept {
    m_shadowResources.release(m_context);
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
                .uv = vertex.uv,
                .tangent = vertex.tangent,
                .bitangentSign = vertex.bitangentSign
            });
        }
        return converted;
    }();

    std::vector<std::uint32_t> indices = mesh.mesh.indices;
    if (indices.empty()) {
        indices = build_default_indices(vertices.size());
    }

    GpuMesh gpuMesh {};
    gpuMesh.model = mesh.transform.matrix();
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

void SceneRenderer::recreate_viewport_resources() {
    m_context.wait_idle();
    destroy_scene_framebuffer();
    destroy_color_resources();
    destroy_depth_resources();
    create_color_resources();
    create_depth_resources();
    create_scene_framebuffer();
    ensure_viewport_descriptor();
}

void SceneRenderer::destroy_viewport_descriptor() noexcept {
    const bool backendReady = (ImGui::GetCurrentContext() != nullptr)
        && (ImGui::GetIO().BackendRendererUserData != nullptr);

    if (m_viewportDescriptor != VK_NULL_HANDLE) {
        if (backendReady) {
            ImGui_ImplVulkan_RemoveTexture(m_viewportDescriptor);
        }
        m_viewportDescriptor = VK_NULL_HANDLE;
    }
    if (m_viewportTexture != nullptr) {
        m_viewportTexture->SetTexID(ImTextureID_Invalid);
        m_viewportTexture->SetStatus(ImTextureStatus_Destroyed);
    }
    if (m_viewportSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_context.device(), m_viewportSampler, nullptr);
        m_viewportSampler = VK_NULL_HANDLE;
    }
    m_viewportImageView = VK_NULL_HANDLE;
}

void SceneRenderer::ensure_viewport_descriptor() {
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    if (io.BackendRendererUserData == nullptr) {
        return;
    }
    if (m_sceneColorImage.image() == VK_NULL_HANDLE) {
        if (m_viewportTexture != nullptr) {
            m_viewportTexture->SetTexID(ImTextureID_Invalid);
            m_viewportTexture->SetStatus(ImTextureStatus_Destroyed);
        }
        return;
    }
    if (m_viewportSampler == VK_NULL_HANDLE) {
        VkSamplerCreateInfo info {};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter = VK_FILTER_LINEAR;
        info.minFilter = VK_FILTER_LINEAR;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.minLod = 0.0F;
        info.maxLod = 0.0F;
        info.maxAnisotropy = 1.0F;
        if (vkCreateSampler(m_context.device(), &info, nullptr, &m_viewportSampler) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create viewport sampler"};
        }
    }

    if (m_viewportTexture == nullptr) {
        m_viewportTexture = IM_NEW(ImTextureData)();
        m_viewportTexture->Format = ImTextureFormat_RGBA32;
        m_viewportTexture->BytesPerPixel = 4;
        m_viewportTexture->UseColors = true;
        ImGui::RegisterUserTexture(m_viewportTexture);
    }
    m_viewportTexture->Width = static_cast<int>(m_viewportExtent.width);
    m_viewportTexture->Height = static_cast<int>(m_viewportExtent.height);

    const VkImageView targetView = m_sceneColorImage.view();
    if (targetView == VK_NULL_HANDLE) {
        m_viewportTexture->SetTexID(ImTextureID_Invalid);
        m_viewportTexture->SetStatus(ImTextureStatus_Destroyed);
        return;
    }

    if (m_viewportDescriptor == VK_NULL_HANDLE) {
        m_viewportDescriptor = ImGui_ImplVulkan_AddTexture(m_viewportSampler, targetView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        m_viewportImageView = targetView;
    } else if (m_viewportImageView != targetView) {
        VkDescriptorImageInfo imageInfo {};
        imageInfo.sampler = m_viewportSampler;
        imageInfo.imageView = targetView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_viewportDescriptor;
        write.dstBinding = 0U;
        write.descriptorCount = 1U;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_context.device(), 1U, &write, 0U, nullptr);
        m_viewportImageView = targetView;
    }

    if (m_viewportDescriptor != VK_NULL_HANDLE) {
        m_viewportTexture->SetTexID(to_texture_id(m_viewportDescriptor));
        m_viewportTexture->SetStatus(ImTextureStatus_OK);
    }
}

void SceneRenderer::record_shadow_pass(VkCommandBuffer commandBuffer) const {
    std::vector<ShadowDrawCommand> commands;
    commands.reserve(m_meshes.size());
    for (const GpuMesh& mesh : m_meshes) {
        if (mesh.indexCount == 0U) {
            continue;
        }
        ShadowDrawCommand command {};
        command.vertexBuffer = mesh.vertexBuffer;
        command.indexBuffer = mesh.indexBuffer;
        command.indexCount = mesh.indexCount;
        command.model = mesh.model;
        commands.push_back(command);
    }

    if (commands.empty()) {
        return;
    }

    for (std::uint32_t slotIndex {0U}; slotIndex < m_shadowResources.activeCount; ++slotIndex) {
        const ShadowSlot& slot = m_shadowResources.slot(slotIndex);
        if (!slot.active || slot.map.render_pass() == VK_NULL_HANDLE) {
            continue;
        }
        slot.pass.record(commandBuffer, slot.map, slot.viewProjection, commands);
    }
}

glm::mat4 SceneRenderer::compute_light_view_projection(const scene::Light& light) const {
    if (!m_sceneBoundsValid) {
        return glm::mat4(1.0F);
    }

    const glm::vec3 direction = glm::length(light.direction) > 0.0F ? glm::normalize(light.direction)
                                                                    : glm::vec3 {0.0F, -1.0F, 0.0F};
    const glm::vec3 center = (m_sceneMin + m_sceneMax) * 0.5F;
    const glm::vec3 extent = (m_sceneMax - m_sceneMin) * 0.5F;
    const float radius = glm::max(glm::length(extent), 1.0F);

    glm::vec3 upReference = glm::abs(direction.y) > 0.9F ? glm::vec3 {0.0F, 0.0F, 1.0F} : glm::vec3 {0.0F, 1.0F, 0.0F};
    glm::vec3 right = glm::normalize(glm::cross(upReference, direction));
    glm::vec3 up = glm::normalize(glm::cross(direction, right));

    const glm::vec3 lightPosition = center - direction * (radius * 2.0F);
    const glm::mat4 view = glm::lookAtRH(lightPosition, center, up);

    const float orthoExtent = radius * 1.5F;
    const float nearPlane = 0.1F;
    const float farPlane = radius * 4.0F;
    const glm::mat4 projection = glm::orthoRH_ZO(-orthoExtent, orthoExtent, -orthoExtent, orthoExtent, nearPlane, farPlane);

    return projection * view;
}

void SceneRenderer::create_light_debug_mesh() {
    const VkDevice device = m_context.device();
    const VkPhysicalDevice physicalDevice = m_context.physical_device();

    const std::array<Vertex, 5> vertices {
        Vertex {.position = glm::vec3 {0.0F, 0.5F, 0.0F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
            .color = glm::vec3 {1.0F, 1.0F, 0.0F}, .uv = glm::vec2 {0.0F, 0.0F},
            .tangent = glm::vec3 {1.0F, 0.0F, 0.0F}, .bitangentSign = 1.0F},
        Vertex {.position = glm::vec3 {0.15F, 0.35F, -0.6F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
            .color = glm::vec3 {1.0F, 1.0F, 0.0F}, .uv = glm::vec2 {0.0F, 0.0F},
            .tangent = glm::vec3 {1.0F, 0.0F, 0.0F}, .bitangentSign = 1.0F},
        Vertex {.position = glm::vec3 {-0.15F, 0.35F, -0.6F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
            .color = glm::vec3 {1.0F, 1.0F, 0.0F}, .uv = glm::vec2 {0.0F, 0.0F},
            .tangent = glm::vec3 {1.0F, 0.0F, 0.0F}, .bitangentSign = 1.0F},
        Vertex {.position = glm::vec3 {-0.15F, 0.15F, -0.6F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
            .color = glm::vec3 {1.0F, 1.0F, 0.0F}, .uv = glm::vec2 {0.0F, 0.0F},
            .tangent = glm::vec3 {1.0F, 0.0F, 0.0F}, .bitangentSign = 1.0F},
        Vertex {.position = glm::vec3 {0.15F, 0.15F, -0.6F}, .normal = glm::vec3 {0.0F, 0.0F, 1.0F},
            .color = glm::vec3 {1.0F, 1.0F, 0.0F}, .uv = glm::vec2 {0.0F, 0.0F},
            .tangent = glm::vec3 {1.0F, 0.0F, 0.0F}, .bitangentSign = 1.0F}
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

    m_gizmoCache.directional.reset();

    destroy_point_light_debug_meshes();
}

void SceneRenderer::destroy_point_light_debug_meshes() noexcept {
    const VkDevice device = m_context.device();
    for (DebugMesh& mesh : m_pointLightDebugMeshes) {
        if (mesh.vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, mesh.vertexBuffer, nullptr);
            mesh.vertexBuffer = VK_NULL_HANDLE;
        }
        if (mesh.vertexMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, mesh.vertexMemory, nullptr);
            mesh.vertexMemory = VK_NULL_HANDLE;
        }
        if (mesh.indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, mesh.indexBuffer, nullptr);
            mesh.indexBuffer = VK_NULL_HANDLE;
        }
        if (mesh.indexMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, mesh.indexMemory, nullptr);
            mesh.indexMemory = VK_NULL_HANDLE;
        }
        mesh.indexCount = 0U;
    }
    m_pointLightDebugMeshes.clear();
    m_gizmoCache.points.clear();
}
} // namespace vulkano::app
