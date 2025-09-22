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
#include <chrono>
#include <string>

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
    void request_close();

private:
    static void framebuffer_resize_callback(GLFWwindow* window, int width, int height);

    void register_callbacks();
    void draw_frame();
    void recreate_swapchain();
    void wait_for_device_idle() const;
    void init_imgui();
    void destroy_imgui();
    void begin_imgui_frame();
    void upload_imgui_fonts();
    void record_command_buffer(std::uint32_t imageIndex);
    void update_timing(double deltaSeconds);
    void create_render_finished_semaphores();
    void destroy_render_finished_semaphores() noexcept;

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
    VkDescriptorPool m_imguiDescriptorPool {VK_NULL_HANDLE};

    std::vector<VkFence> m_imagesInFlight {};
    std::vector<VkSemaphore> m_renderFinishedSemaphores {};
    std::uint32_t m_currentFrame {0U};
    bool m_framebufferResized {false};
    std::chrono::steady_clock::time_point m_lastFrameTime {std::chrono::steady_clock::now()};
    double m_frameTimeMs {0.0};
    double m_fps {0.0};
    std::string m_deviceName;

    glm::vec4 m_triangleColor {1.0F, 1.0F, 1.0F, 1.0F};
};

} // namespace vulkano
