#pragma once

#include <vulkano/app_config.hpp>
#include <vulkano/buffer.hpp>
#include <vulkano/command_allocator.hpp>
#include <vulkano/framebuffers.hpp>
#include <vulkano/glfw_context.hpp>
#include <vulkano/graphics_pipeline.hpp>
#include <vulkano/render_pass.hpp>
#include <vulkano/swapchain.hpp>
#include <vulkano/synchronization.hpp>
#include <vulkano/vulkan_context.hpp>
#include <vulkano/window.hpp>

#include <glm/vec4.hpp>
#include <vector>

namespace vulkano {

class VulkanApplication final {
public:
    explicit VulkanApplication(const AppConfig& config);
    VulkanApplication(const VulkanApplication&) = delete;
    VulkanApplication(VulkanApplication&&) = delete;
    auto operator=(const VulkanApplication&) -> VulkanApplication& = delete;
    auto operator=(VulkanApplication&&) -> VulkanApplication& = delete;
    ~VulkanApplication();

    void run();

private:
    static void framebuffer_resize_callback(GLFWwindow* window, int width, int height);

    void register_callbacks();
    void draw_frame();
    void recreate_swapchain();
    void rebuild_command_buffers();
    void wait_for_device_idle() const;

    AppConfig m_config;
    GlfwContext m_glfwContext;
    Window m_window;
    VulkanContext m_context;
    Swapchain m_swapchain;
    RenderPass m_renderPass;
    FramebufferCollection m_framebuffers;
    CommandAllocator m_commandAllocator;
    SyncManager m_syncManager;
    GraphicsPipeline m_pipeline;
    Buffer m_vertexBuffer;

    std::vector<VkFence> m_imagesInFlight {};
    std::uint32_t m_currentFrame {0U};
    bool m_framebufferResized {false};

    glm::vec4 m_triangleColor {1.0F, 1.0F, 1.0F, 1.0F};
};

} // namespace vulkano
