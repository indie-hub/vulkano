#pragma once

#include <vulkano/app_config.hpp>
#include <vulkano/buffer.hpp>
#include <vulkano/command_allocator.hpp>
#include <vulkano/framebuffers.hpp>
#include <vulkano/glfw_context.hpp>
#include <vulkano/graphics_pipeline.hpp>
#include <vulkano/depth_resources.hpp>
#include <vulkano/mesh_gpu.hpp>
#include <vulkano/primitives.hpp>
#include <vulkano/render_pass.hpp>
#include <vulkano/shadow_map.hpp>
#include <vulkano/shadow_pipeline.hpp>
#include <vulkano/shadow_render_pass.hpp>
#include <vulkano/swapchain.hpp>
#include <vulkano/synchronization.hpp>
#include <vulkano/vulkan_context.hpp>
#include <vulkano/window.hpp>

#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/constants.hpp>

#include <memory>
#include <vector>
#include <chrono>
#include <string>

struct ImGuiIO;

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
    [[nodiscard]] auto primitive_count() const noexcept -> std::size_t;
    [[nodiscard]] auto scene_light_position() const noexcept -> glm::vec3;
    [[nodiscard]] auto scene_light_intensity() const noexcept -> float;

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
    void update_camera_input(double deltaSeconds, const ImGuiIO& io);
    void apply_scroll_input(const ImGuiIO& io);
    void create_render_finished_semaphores();
    void destroy_render_finished_semaphores() noexcept;
    void create_shadow_framebuffer();
    void destroy_shadow_framebuffer() noexcept;
    void initialise_scene();
    void rebuild_dirty_meshes();
    void create_descriptor_resources();
    void destroy_descriptor_resources() noexcept;
    void update_global_uniforms();
    [[nodiscard]] auto camera_position() const noexcept -> glm::vec3;
    [[nodiscard]] auto camera_forward() const noexcept -> glm::vec3;
    [[nodiscard]] auto camera_right(const glm::vec3& forward) const noexcept -> glm::vec3;
    [[nodiscard]] auto camera_up(const glm::vec3& forward, const glm::vec3& right) const noexcept -> glm::vec3;
    void set_cursor_mode(int mode) noexcept;
    static void scroll_callback(GLFWwindow* window, double xOffset, double yOffset);
    [[nodiscard]] auto compute_model_matrix(const PrimitiveProperties& properties) const noexcept -> glm::mat4;

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

    struct CameraState final {
        static constexpr glm::vec3 defaultPosition {0.0F, 1.6F, 6.0F};
        static constexpr float defaultYaw {-glm::half_pi<float>()};
        static constexpr float defaultPitch {0.0F};
        static constexpr float defaultFovY {glm::pi<float>() / 3.0F};
        static constexpr float defaultNearPlane {0.1F};
        static constexpr float defaultFarPlane {100.0F};

        glm::vec3 position {defaultPosition};
        float yaw {defaultYaw};
        float pitch {defaultPitch};
        float fovY {defaultFovY};
        float nearPlane {defaultNearPlane};
        float farPlane {defaultFarPlane};
    };

    SceneState m_scene {};
    CameraState m_camera {};
    DepthResources m_depthResources {};
    ShadowMap m_shadowMap {};
    ShadowRenderPass m_shadowRenderPass {};
    ShadowPipeline m_shadowPipeline {};
    VkFramebuffer m_shadowFramebuffer {VK_NULL_HANDLE};
    VkFormat m_depthFormat {VK_FORMAT_D32_SFLOAT};
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
    double m_deltaSeconds {0.0};
    double m_scrollDelta {0.0};
    bool m_cameraLookActive {false};
    bool m_cameraLock {false};
    bool m_firstMouse {true};
    glm::dvec2 m_lastCursorPosition {0.0, 0.0};
    std::string m_deviceName;
};

} // namespace vulkano
