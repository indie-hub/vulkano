#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include <glm/mat4x4.hpp>

#include <vulkano/scene/material.hpp>
#include <vulkano/scene/light.hpp>
#include <vulkano/scene/mesh.hpp>
#include <vulkano/scene/transform.hpp>
#include <vulkano/vk/color_image.hpp>
#include <vulkano/vk/depth_image.hpp>
#include <vulkano/app/shadow_resources.hpp>

namespace vulkano::app {
class VulkanContext;
class Window;
class MaterialBuffer;
class MaterialTextureCache;
class LightBuffer;

class SceneRenderer final {
public:
    struct SceneMesh final {
        scene::MeshData mesh {};
        scene::Transform transform {};
        scene::MaterialId material {scene::MaterialId::invalid()};
    };

    struct SceneNode final {
        std::string name {};
        scene::Transform transform {};
        scene::MeshData mesh {};
        scene::MaterialId material {scene::MaterialId::invalid()};
        std::vector<SceneNode> children;

        [[nodiscard]] bool has_geometry() const noexcept {
            return material != scene::MaterialId::invalid();
        }
    };

    SceneRenderer(const VulkanContext& context, const Window& window, VkDescriptorSetLayout ssaoLayout = VK_NULL_HANDLE);
    ~SceneRenderer() noexcept;

    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;
    SceneRenderer(SceneRenderer&&) noexcept = delete;
    SceneRenderer& operator=(SceneRenderer&&) noexcept = delete;

    void set_scene(const std::vector<SceneMesh>& meshes);
    void set_scene_graph(const SceneNode& root);
    void set_material_resources(const MaterialBuffer& buffer, const MaterialTextureCache& textures);
    void set_light_resources(LightBuffer& buffer, const scene::LightRegistry& registry);
    void set_show_light_debug(bool enabled) noexcept;
    void record_shadow_pass(VkCommandBuffer commandBuffer) const;

    [[nodiscard]] VkRenderPass render_pass() const noexcept;
    [[nodiscard]] VkPipeline pipeline() const noexcept;
    [[nodiscard]] VkPipelineLayout pipeline_layout() const noexcept;
    [[nodiscard]] const std::vector<VkFramebuffer>& framebuffers() const noexcept;
    [[nodiscard]] VkImageView albedo_image_view() const noexcept;
    [[nodiscard]] VkImageView normal_image_view() const noexcept;
    [[nodiscard]] VkFormat albedo_format() const noexcept;
    [[nodiscard]] VkFormat normal_format() const noexcept;
    [[nodiscard]] VkImageView linear_depth_image_view() const noexcept;
    [[nodiscard]] VkFormat linear_depth_format() const noexcept;
    [[nodiscard]] float shadow_bias() const noexcept;
    [[nodiscard]] float shadow_pcf_radius() const noexcept;
    [[nodiscard]] bool shadows_enabled() const noexcept;
    [[nodiscard]] bool shadow_debug_enabled() const noexcept;
    [[nodiscard]] std::uint32_t shadow_slot_capacity() const noexcept;
    [[nodiscard]] std::uint32_t shadow_active_caster_count() const noexcept;
    [[nodiscard]] VkExtent2D shadow_map_extent() const noexcept;
    [[nodiscard]] std::optional<std::uint32_t> shadow_slot_for_light(scene::LightId id) const noexcept;
    void set_shadow_bias(float bias) noexcept;
    void set_shadow_pcf_radius(float radius) noexcept;
    void set_shadows_enabled(bool enabled) noexcept;
    void set_shadow_debug_enabled(bool enabled) noexcept;

    using CommandRecorder = std::function<void(VkCommandBuffer)>;
    void record_command_buffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex,
        const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPosition,
        const CommandRecorder& overlayRecorder,
        VkDescriptorSet ssaoDescriptor = VK_NULL_HANDLE) const;

private:
    struct GpuMesh final {
        VkBuffer vertexBuffer {VK_NULL_HANDLE};
        VkDeviceMemory vertexMemory {VK_NULL_HANDLE};
        VkBuffer indexBuffer {VK_NULL_HANDLE};
        VkDeviceMemory indexMemory {VK_NULL_HANDLE};
        std::uint32_t indexCount {0U};
        glm::mat4 model {1.0F};
        scene::MaterialId material {scene::MaterialId::invalid()};
    };

    struct DebugMesh final {
        VkBuffer vertexBuffer {VK_NULL_HANDLE};
        VkDeviceMemory vertexMemory {VK_NULL_HANDLE};
        VkBuffer indexBuffer {VK_NULL_HANDLE};
        VkDeviceMemory indexMemory {VK_NULL_HANDLE};
        std::uint32_t indexCount {0U};
        glm::mat4 model {1.0F};
    };

    struct LightGizmoHandle final {
        scene::LightId id {scene::LightId::invalid()};
        DebugMesh* mesh {nullptr};
        glm::mat4 transform {1.0F};
        scene::LightType type {scene::LightType::Directional};
        bool dirty {false};
    };

    struct GizmoCache final {
        std::optional<LightGizmoHandle> directional {};
        std::vector<LightGizmoHandle> points;

        void release() noexcept;
    };

    void create_render_pass();
    void create_pipeline_layout();
    void create_graphics_pipeline();
    void create_framebuffers();
    void destroy_framebuffers() noexcept;
    void destroy_meshes() noexcept;
    void create_depth_resources();
    void destroy_depth_resources() noexcept;
    void create_color_resources();
    void destroy_color_resources() noexcept;
    void create_material_descriptors();
    void destroy_material_descriptors() noexcept;
    void create_light_descriptors();
    void destroy_light_descriptors() noexcept;
    void create_light_debug_mesh();
    void destroy_light_debug_mesh() noexcept;
    void create_shadow_resources();
    void destroy_shadow_resources() noexcept;
    void destroy_point_light_debug_meshes() noexcept;
    [[nodiscard]] glm::mat4 compute_light_view_projection(const scene::Light& light) const;

    void upload_mesh(const SceneMesh& mesh);

    const VulkanContext& m_context;
    VkRenderPass m_renderPass {VK_NULL_HANDLE};
    VkPipelineLayout m_pipelineLayout {VK_NULL_HANDLE};
    VkPipeline m_pipeline {VK_NULL_HANDLE};
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<GpuMesh> m_meshes;
    DebugMesh m_lightDebugMesh;
    std::vector<DebugMesh> m_pointLightDebugMeshes;
    GizmoCache m_gizmoCache;
    VkFormat m_depthFormat {VK_FORMAT_UNDEFINED};
    VkFormat m_albedoFormat {VK_FORMAT_R8G8B8A8_UNORM};
    VkFormat m_normalFormat {VK_FORMAT_R8G8B8A8_UNORM};
    VkFormat m_shadowFormat {VK_FORMAT_D32_SFLOAT};
    vk::DepthImage m_depthImage;
    vk::ColorImage m_albedoImage;
    vk::ColorImage m_normalImage;
    vk::ColorImage m_linearDepthImage;
    VkFormat m_linearDepthFormat {VK_FORMAT_R32_SFLOAT};
    VkDescriptorSetLayout m_descriptorLayout {VK_NULL_HANDLE};
    VkDescriptorSetLayout m_materialDescriptorLayout {VK_NULL_HANDLE};
    VkDescriptorPool m_materialDescriptorPool {VK_NULL_HANDLE};
    VkDescriptorSet m_materialDescriptorSet {VK_NULL_HANDLE};
    const MaterialBuffer* m_materialBuffer {nullptr};
    std::vector<VkDescriptorImageInfo> m_materialTextureInfos;
    VkDescriptorSetLayout m_lightDescriptorLayout {VK_NULL_HANDLE};
    VkDescriptorPool m_lightDescriptorPool {VK_NULL_HANDLE};
    VkDescriptorSet m_lightDescriptorSet {VK_NULL_HANDLE};
    const LightBuffer* m_lightBuffer {nullptr};
    bool m_showLightDebug {false};
    glm::vec3 m_lightDirection {0.0F, -1.0F, 0.0F};
    glm::vec3 m_lightColor {1.0F, 1.0F, 1.0F};
    float m_lightIntensity {1.0F};
    VkExtent2D m_shadowExtent {2048U, 2048U};
    mutable ShadowResources m_shadowResources;
    glm::vec3 m_sceneMin {0.0F, 0.0F, 0.0F};
    glm::vec3 m_sceneMax {0.0F, 0.0F, 0.0F};
    bool m_sceneBoundsValid {false};
    float m_shadowBias {0.002F};
    float m_shadowPcfRadius {1.0F};
    bool m_shadowsEnabled {true};
    bool m_shadowDebug {false};
    bool m_primaryLightCastsShadow {true};
};
} // namespace vulkano::app
