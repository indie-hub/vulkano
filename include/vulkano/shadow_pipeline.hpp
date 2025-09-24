#pragma once

#include <vulkano/shadow_render_pass.hpp>
#include <vulkano/vulkan_context.hpp>

#include <vulkan/vulkan.h>

namespace vulkano {

class ShadowPipeline final {
public:
    ShadowPipeline() = default;
    ShadowPipeline(const ShadowPipeline&) = delete;
    ShadowPipeline(ShadowPipeline&& other) noexcept;
    auto operator=(const ShadowPipeline&) -> ShadowPipeline& = delete;
    auto operator=(ShadowPipeline&& other) noexcept -> ShadowPipeline&;
    ~ShadowPipeline() noexcept;

    [[nodiscard]] static auto create(
        const VulkanContext& context,
        const ShadowRenderPass& renderPass,
        VkDescriptorSetLayout descriptorSetLayout) -> ShadowPipeline;

    void recreate(
        const VulkanContext& context,
        const ShadowRenderPass& renderPass,
        VkDescriptorSetLayout descriptorSetLayout);

    [[nodiscard]] auto handle() const noexcept -> VkPipeline;
    [[nodiscard]] auto layout() const noexcept -> VkPipelineLayout;

private:
    ShadowPipeline(
        const VulkanContext& context,
        const ShadowRenderPass& renderPass,
        VkDescriptorSetLayout descriptorSetLayout);

    void initialise(
        const VulkanContext& context,
        const ShadowRenderPass& renderPass,
        VkDescriptorSetLayout descriptorSetLayout);
    void cleanup() noexcept;
    void move_from(ShadowPipeline&& other) noexcept;

    VkDevice m_device {VK_NULL_HANDLE};
    VkPipelineLayout m_layout {VK_NULL_HANDLE};
    VkPipeline m_pipeline {VK_NULL_HANDLE};
};

} // namespace vulkano

