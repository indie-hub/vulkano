#pragma once

#include <vulkan/vulkan.h>

#include <vulkano/vk/depth_image.hpp>

namespace vulkano::app {
class VulkanContext;

class ShadowMap final {
public:
    ShadowMap() = default;
    ShadowMap(const VulkanContext& context, VkExtent2D extent, VkFormat depthFormat);
    ~ShadowMap() noexcept;

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;
    ShadowMap(ShadowMap&&) noexcept;
    ShadowMap& operator=(ShadowMap&&) noexcept;

    void recreate(const VulkanContext& context, VkExtent2D extent, VkFormat depthFormat);

    [[nodiscard]] VkFramebuffer framebuffer() const noexcept;
    [[nodiscard]] VkRenderPass render_pass() const noexcept;
    [[nodiscard]] VkExtent2D extent() const noexcept;
    [[nodiscard]] VkDescriptorImageInfo descriptor_info() const noexcept;
    [[nodiscard]] VkImage image() const noexcept;
    [[nodiscard]] VkImageView view() const noexcept;

private:
    void destroy() noexcept;
    void create_shadow_resources(const VulkanContext& context, VkExtent2D extent, VkFormat depthFormat);

    const VulkanContext* m_context {nullptr};

    VkRenderPass m_renderPass {VK_NULL_HANDLE};
    VkFramebuffer m_framebuffer {VK_NULL_HANDLE};
    vk::DepthImage m_depthImage;
    VkSampler m_sampler {VK_NULL_HANDLE};
    VkExtent2D m_extent {0U, 0U};
};
} // namespace vulkano::app
