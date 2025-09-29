#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include <glm/mat4x4.hpp>

namespace vulkano::app {
class VulkanContext;
class Window;

struct TrianglePushConstants final {
    glm::mat4 model {};
    glm::mat4 view {};
    glm::mat4 projection {};
};

class TriangleRenderer final {
public:
    TriangleRenderer(const VulkanContext& context, const Window& window);
    ~TriangleRenderer() noexcept;

    TriangleRenderer(const TriangleRenderer&) = delete;
    TriangleRenderer& operator=(const TriangleRenderer&) = delete;
    TriangleRenderer(TriangleRenderer&&) noexcept = delete;
    TriangleRenderer& operator=(TriangleRenderer&&) noexcept = delete;

    [[nodiscard]] VkRenderPass render_pass() const noexcept;
    [[nodiscard]] VkPipeline pipeline() const noexcept;
    [[nodiscard]] VkPipelineLayout pipeline_layout() const noexcept;
    [[nodiscard]] const std::vector<VkFramebuffer>& framebuffers() const noexcept;
    [[nodiscard]] TrianglePushConstants push_constants() const noexcept;
    [[nodiscard]] VkBuffer vertex_buffer() const noexcept;
    [[nodiscard]] std::uint32_t vertex_count() const noexcept;

    void record_command_buffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex) const;

private:
    void create_render_pass();
    void create_pipeline_layout();
    void create_graphics_pipeline();
    void create_framebuffers();
    void create_vertex_buffer();
    void configure_push_constants(const Window& window) noexcept;

    const VulkanContext& m_context;
    VkRenderPass m_renderPass {VK_NULL_HANDLE};
    VkPipelineLayout m_pipelineLayout {VK_NULL_HANDLE};
    VkPipeline m_pipeline {VK_NULL_HANDLE};
    VkBuffer m_vertexBuffer {VK_NULL_HANDLE};
    VkDeviceMemory m_vertexMemory {VK_NULL_HANDLE};
    std::uint32_t m_vertexCount {0U};
    std::vector<VkFramebuffer> m_framebuffers;
    TrianglePushConstants m_pushConstants {};
};
} // namespace vulkano::app
