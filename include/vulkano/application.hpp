#pragma once

#include <vulkano/app_config.hpp>
#include <vulkano/buffer.hpp>
#include <vulkano/command_allocator.hpp>
#include <vulkano/framebuffers.hpp>
#include <vulkano/glfw_context.hpp>
#include <vulkano/graphics_pipeline.hpp>
#include <vulkano/mesh_gpu.hpp>
#include <vulkano/primitives.hpp>
#include <vulkano/render_pass.hpp>
#include <vulkano/swapchain.hpp>
#include <vulkano/synchronization.hpp>
#include <vulkano/vulkan_context.hpp>
#include <vulkano/window.hpp>

#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include <memory>
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
    void initialise_scene();
    void rebuild_dirty_meshes();
    void create_descriptor_resources();
    void destroy_descriptor_resources() noexcept;
    void update_global_uniforms();

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
    VkDescriptorPool m_imguiDescriptorPool {VK_NULL_HANDLE};

    struct ScenePrimitive final {
        std::unique_ptr<Primitive> primitive {};
        MeshGpuResources gpu {};
    };

    struct SceneState final {
        glm::vec3 lightPosition {2.0F, 4.0F, 2.0F};
        float lightIntensity {1.0F};
        std::vector<ScenePrimitive> primitives {};
    };

    SceneState m_scene {};
    VkDescriptorSetLayout m_descriptorSetLayout {VK_NULL_HANDLE};
    VkDescriptorPool m_descriptorPool {VK_NULL_HANDLE};
    VkDescriptorSet m_descriptorSet {VK_NULL_HANDLE};
    Buffer m_globalUniformBuffer;

    std::vector<VkFence> m_imagesInFlight {};
    std::vector<VkSemaphore> m_renderFinishedSemaphores {};
    std::uint32_t m_currentFrame {0U};
    bool m_framebufferResized {false};
    std::chrono::steady_clock::time_point m_lastFrameTime {std::chrono::steady_clock::now()};
    double m_frameTimeMs {0.0};
    double m_fps {0.0};
    std::string m_deviceName;
};

} // namespace vulkano
