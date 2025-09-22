#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include <vulkano/vulkan_context.hpp>

namespace vulkano {

class ShadowRenderPass final {
public:
    ShadowRenderPass() = default;
    ShadowRenderPass(const ShadowRenderPass&) = delete;
    ShadowRenderPass(ShadowRenderPass&& other) noexcept;
    auto operator=(const ShadowRenderPass&) -> ShadowRenderPass& = delete;
    auto operator=(ShadowRenderPass&& other) noexcept -> ShadowRenderPass&;
    ~ShadowRenderPass() noexcept;

    [[nodiscard]] static auto create(const VulkanContext& context, VkFormat depthFormat) -> ShadowRenderPass;

    void recreate(const VulkanContext& context, VkFormat depthFormat);
    void cleanup() noexcept;

    [[nodiscard]] auto handle() const noexcept -> VkRenderPass;

private:
    ShadowRenderPass(const VulkanContext& context, VkFormat depthFormat);

    void initialise(const VulkanContext& context, VkFormat depthFormat);
    void move_from(ShadowRenderPass&& other) noexcept;

    VkDevice m_device {VK_NULL_HANDLE};
    VkRenderPass m_renderPass {VK_NULL_HANDLE};
};

} // namespace vulkano

