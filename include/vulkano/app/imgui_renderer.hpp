#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

namespace vulkano::app {
class VulkanContext;
class Window;

class ImGuiRenderer final {
public:
    ImGuiRenderer(const VulkanContext& context, const Window& window, VkRenderPass renderPass);
    ~ImGuiRenderer() noexcept;

    ImGuiRenderer(const ImGuiRenderer&) = delete;
    ImGuiRenderer& operator=(const ImGuiRenderer&) = delete;
    ImGuiRenderer(ImGuiRenderer&&) noexcept = delete;
    ImGuiRenderer& operator=(ImGuiRenderer&&) noexcept = delete;

    void begin_frame() noexcept;
    void end_frame() noexcept;
    void render(VkCommandBuffer commandBuffer) const;
    void update_metrics(float deltaTimeSeconds) noexcept;
    void draw_overlay() const noexcept;

private:
    void create_descriptor_pool();
    void upload_fonts();
    void destroy_descriptor_pool() noexcept;
    void configure_style() noexcept;

    const VulkanContext& m_context;
    VkDescriptorPool m_descriptorPool {VK_NULL_HANDLE};
    VkRenderPass m_renderPass {VK_NULL_HANDLE};
    float m_frameTimeMilliseconds {0.0F};
    float m_framesPerSecond {0.0F};
};
} // namespace vulkano::app
