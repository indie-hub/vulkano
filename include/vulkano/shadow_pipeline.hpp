#pragma once

#include <cstdint>

#include <glm/mat4x4.hpp>

#include <vulkan/vulkan.h>

#include <span>

#include <vulkano/shadow_render_pass.hpp>
#include <vulkano/vulkan_context.hpp>

namespace vulkano {

struct alignas(16) ShadowPushConstants final {
    ShadowPushConstants();

    glm::mat4 model;
    std::uint32_t cascadeIndex;
    float padding0;
    float padding1;
    float padding2;
};

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
        std::span<const VkDescriptorSetLayout> descriptorSetLayouts) -> ShadowPipeline;

    void recreate(
        const VulkanContext& context,
        const ShadowRenderPass& renderPass,
        std::span<const VkDescriptorSetLayout> descriptorSetLayouts);

    void cleanup() noexcept;

    [[nodiscard]] auto handle() const noexcept -> VkPipeline;
    [[nodiscard]] auto layout() const noexcept -> VkPipelineLayout;

private:
    ShadowPipeline(
        const VulkanContext& context,
        const ShadowRenderPass& renderPass,
        std::span<const VkDescriptorSetLayout> descriptorSetLayouts);

    void initialise(
        const VulkanContext& context,
        const ShadowRenderPass& renderPass,
        std::span<const VkDescriptorSetLayout> descriptorSetLayouts);
    void move_from(ShadowPipeline&& other) noexcept;

    VkDevice m_device {VK_NULL_HANDLE};
    VkPipelineLayout m_layout {VK_NULL_HANDLE};
    VkPipeline m_pipeline {VK_NULL_HANDLE};
};

} // namespace vulkano
