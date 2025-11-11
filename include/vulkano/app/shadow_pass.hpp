#pragma once

#include <vector>

#include <glm/mat4x4.hpp>

#include <vulkan/vulkan.h>

namespace vulkano::app {
class VulkanContext;
class ShadowMap;

struct ShadowDrawCommand final {
    VkBuffer vertexBuffer {VK_NULL_HANDLE};
    VkBuffer indexBuffer {VK_NULL_HANDLE};
    VkDeviceSize vertexOffset {0U};
    VkDeviceSize indexOffset {0U};
    std::uint32_t indexCount {0U};
    glm::mat4 model {1.0F};
};

class ShadowPass final {
public:
    ShadowPass() = default;
    ShadowPass(const VulkanContext& context, VkRenderPass renderPass);
    ~ShadowPass() noexcept;

    ShadowPass(const ShadowPass&) = delete;
    ShadowPass& operator=(const ShadowPass&) = delete;
    ShadowPass(ShadowPass&&) noexcept;
    ShadowPass& operator=(ShadowPass&&) noexcept;

    void recreate(const VulkanContext& context, VkRenderPass renderPass);

    void record(VkCommandBuffer commandBuffer, const ShadowMap& shadowMap, const glm::mat4& lightViewProjection,
        const std::vector<ShadowDrawCommand>& commands) const;

private:
    void destroy() noexcept;
    void create_pipeline(const VulkanContext& context, VkRenderPass renderPass);

    const VulkanContext* m_context {nullptr};
    VkPipelineLayout m_pipelineLayout {VK_NULL_HANDLE};
    VkPipeline m_pipeline {VK_NULL_HANDLE};
};
} // namespace vulkano::app
