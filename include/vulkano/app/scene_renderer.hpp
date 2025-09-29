#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include <vulkan/vulkan.h>

#include <glm/mat4x4.hpp>

#include <vulkano/scene/mesh.hpp>

namespace vulkano::app {
class VulkanContext;
class Window;

class SceneRenderer final {
public:
    struct SceneMesh final {
        scene::MeshData mesh {};
        glm::mat4 model {1.0F};
    };

    SceneRenderer(const VulkanContext& context, const Window& window);
    ~SceneRenderer() noexcept;

    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;
    SceneRenderer(SceneRenderer&&) noexcept = delete;
    SceneRenderer& operator=(SceneRenderer&&) noexcept = delete;

    void set_scene(const std::vector<SceneMesh>& meshes);

    [[nodiscard]] VkRenderPass render_pass() const noexcept;
    [[nodiscard]] VkPipeline pipeline() const noexcept;
    [[nodiscard]] VkPipelineLayout pipeline_layout() const noexcept;
    [[nodiscard]] const std::vector<VkFramebuffer>& framebuffers() const noexcept;

    using CommandRecorder = std::function<void(VkCommandBuffer)>;
    void record_command_buffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex,
        const glm::mat4& view, const glm::mat4& projection, const CommandRecorder& overlayRecorder) const;

private:
    struct GpuMesh final {
        VkBuffer vertexBuffer {VK_NULL_HANDLE};
        VkDeviceMemory vertexMemory {VK_NULL_HANDLE};
        VkBuffer indexBuffer {VK_NULL_HANDLE};
        VkDeviceMemory indexMemory {VK_NULL_HANDLE};
        std::uint32_t indexCount {0U};
        glm::mat4 model {1.0F};
    };

    void create_render_pass();
    void create_pipeline_layout();
    void create_graphics_pipeline();
    void create_framebuffers();
    void destroy_framebuffers() noexcept;
    void destroy_meshes() noexcept;

    void upload_mesh(const SceneMesh& mesh);

    const VulkanContext& m_context;
    VkRenderPass m_renderPass {VK_NULL_HANDLE};
    VkPipelineLayout m_pipelineLayout {VK_NULL_HANDLE};
    VkPipeline m_pipeline {VK_NULL_HANDLE};
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<GpuMesh> m_meshes;
};
} // namespace vulkano::app
