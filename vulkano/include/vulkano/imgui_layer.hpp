#pragma once

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace vulkano {

class VulkanContext;

class ImGuiLayer final {
public:
    ImGuiLayer(VulkanContext& ctx, VkRenderPass render_pass, uint32_t image_count);
    ~ImGuiLayer();

    void new_frame();
    void render(VkCommandBuffer cmd);
    void shutdown();

private:
    class Impl;
    Impl* m_impl;
};

} // namespace vulkano

