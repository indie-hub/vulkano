#pragma once

#include <vulkano/vulkan_context.hpp>

#include <vulkan/vulkan.h>

namespace vulkano {

class RenderPass final {
public:
    RenderPass() = default;
    RenderPass(const RenderPass&) = delete;
    RenderPass(RenderPass&& other) noexcept;
    auto operator=(const RenderPass&) -> RenderPass& = delete;
    auto operator=(RenderPass&& other) noexcept -> RenderPass&;
    ~RenderPass() noexcept;

    [[nodiscard]] static auto create(const VulkanContext& context, VkFormat swapchainFormat) -> RenderPass;

    [[nodiscard]] auto handle() const noexcept -> VkRenderPass;

private:
    explicit RenderPass(const VulkanContext& context, VkFormat swapchainFormat);

    void initialise(const VulkanContext& context, VkFormat swapchainFormat);
    void cleanup() noexcept;
    void move_from(RenderPass&& other) noexcept;

    VkDevice m_device {VK_NULL_HANDLE};
    VkRenderPass m_renderPass {VK_NULL_HANDLE};
};

} // namespace vulkano
