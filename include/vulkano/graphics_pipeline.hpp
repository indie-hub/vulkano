#pragma once

#include <vulkano/render_pass.hpp>
#include <vulkano/vulkan_context.hpp>
#include <vulkano/vertex.hpp>

#include <vulkan/vulkan.h>

namespace vulkano {

class GraphicsPipeline final {
public:
    GraphicsPipeline() = default;
    GraphicsPipeline(const GraphicsPipeline&) = delete;
    GraphicsPipeline(GraphicsPipeline&& other) noexcept;
    auto operator=(const GraphicsPipeline&) -> GraphicsPipeline& = delete;
    auto operator=(GraphicsPipeline&& other) noexcept -> GraphicsPipeline&;
    ~GraphicsPipeline() noexcept;

    [[nodiscard]] static auto create(const VulkanContext& context, const RenderPass& renderPass, VkDescriptorSetLayout descriptorSetLayout) -> GraphicsPipeline;

    void recreate(const VulkanContext& context, const RenderPass& renderPass, VkDescriptorSetLayout descriptorSetLayout);

    [[nodiscard]] auto handle() const noexcept -> VkPipeline;
    [[nodiscard]] auto layout() const noexcept -> VkPipelineLayout;

private:
    GraphicsPipeline(const VulkanContext& context, const RenderPass& renderPass, VkDescriptorSetLayout descriptorSetLayout);

    void initialise(const VulkanContext& context, const RenderPass& renderPass, VkDescriptorSetLayout descriptorSetLayout);
    void cleanup() noexcept;
    void move_from(GraphicsPipeline&& other) noexcept;

    VkDevice m_device {VK_NULL_HANDLE};
    VkPipelineLayout m_layout {VK_NULL_HANDLE};
    VkPipeline m_pipeline {VK_NULL_HANDLE};
};

} // namespace vulkano
