#pragma once

#include <vulkano/app_config.hpp>
#include <vulkano/buffer.hpp>
#include <vulkano/cascaded_shadow_map.hpp>
#include <vulkano/command_allocator.hpp>
#include <vulkano/depth_resources.hpp>
#include <vulkano/framebuffers.hpp>
#include <vulkano/glfw_context.hpp>
#include <vulkano/graphics_pipeline.hpp>
#include <vulkano/mesh_gpu.hpp>
#include <vulkano/primitives.hpp>
#include <vulkano/render_pass.hpp>
#include <vulkano/shadow_pipeline.hpp>
#include <vulkano/shadow_render_pass.hpp>
#include <vulkano/swapchain.hpp>
#include <vulkano/synchronization.hpp>
#include <vulkano/texture.hpp>
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
#include <array>

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
    [[nodiscard]] auto scene_light_direction() const noexcept -> glm::vec3;
    [[nodiscard]] auto scene_light_position() const noexcept -> glm::vec3;
    [[nodiscard]] auto scene_light_intensity() const noexcept -> float;

private:
    static constexpr std::uint32_t maxFramesInFlight {2U};

    struct ScenePrimitive;

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
    void initialise_scene();
    void rebuild_dirty_meshes();
    void create_descriptor_resources();
    void update_descriptor_set_bindings(std::uint32_t frameIndex);
    void destroy_descriptor_resources() noexcept;
    void mark_all_frame_descriptors_dirty() noexcept;
    void create_texture_resources();
    void destroy_texture_resources() noexcept;
    void allocate_material_descriptor_pool(std::uint32_t primitiveCount);
    void destroy_material_descriptor_pool() noexcept;
    void update_material_descriptor(ScenePrimitive& primitive);
    void update_all_material_descriptors();
    void update_global_uniforms(std::uint32_t frameIndex);
    void create_shadow_resources();
    void destroy_shadow_resources() noexcept;
    void render_shadow_pass(VkCommandBuffer commandBuffer, std::uint32_t frameIndex);
    void ensure_shadow_resources(std::uint32_t frameIndex);
    void recreate_shadow_resources(std::uint32_t cascadeCount, std::uint32_t resolution);
    void clear_shadow_debug_textures() noexcept;
    void update_shadow_debug_textures();
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
    ShadowRenderPass m_shadowRenderPass;
    ShadowPipeline m_shadowPipeline;
    CascadedShadowMapResources m_shadowMapResources;
    VkDescriptorPool m_imguiDescriptorPool {VK_NULL_HANDLE};

    struct ScenePrimitive final {
        std::unique_ptr<Primitive> primitive {};
        MeshGpuResources gpu {};

        struct MaterialTextures final {
            TextureImage customAlbedo {};
            TextureImage customNormal {};
            const TextureImage* boundAlbedo {nullptr};
            const TextureImage* boundNormal {nullptr};
            VkDescriptorSet descriptor {VK_NULL_HANDLE};
            bool albedoUsesFallback {true};
            bool normalUsesFallback {true};
            VkExtent2D albedoExtent {0U, 0U};
            VkExtent2D normalExtent {0U, 0U};
        } material;
    };

    struct SceneState final {
        glm::vec3 lightDirection {2.0F, 4.0F, 2.0F};
        float lightIntensity {1.0F};
        std::vector<ScenePrimitive> primitives {};
    };

    struct FrameResources final {
        Buffer uniformBuffer {};
        VkDescriptorSet descriptor {VK_NULL_HANDLE};
        bool descriptorDirty {true};
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

    struct ShadowState final {
        CascadedShadowSettings settings {};
        std::uint32_t cascadeCount {maxShadowCascades};
        std::uint32_t resolution {2048U};
        bool resourcesDirty {false};
        bool descriptorDirty {true};
        bool debugDescriptorsDirty {true};
        bool firstUse {true};
        VkImageLayout currentLayout {VK_IMAGE_LAYOUT_UNDEFINED};
        VkImageLayout sampleLayout {VK_IMAGE_LAYOUT_UNDEFINED};
        std::array<float, maxShadowCascades> cascadeTexelSizes {};
        std::array<float, maxShadowCascades> cascadeRadii {};
        std::vector<VkDescriptorSet> debugAtlasDescriptors {};
        VkFormat format {VK_FORMAT_D32_SFLOAT};
    };

    SceneState m_scene {};
    CameraState m_camera {};
    ShadowState m_shadow {};
    DepthResources m_depthResources {};
    VkFormat m_depthFormat {VK_FORMAT_D32_SFLOAT};
    VkDescriptorSetLayout m_descriptorSetLayout {VK_NULL_HANDLE};
    VkDescriptorSetLayout m_materialDescriptorSetLayout {VK_NULL_HANDLE};
    VkDescriptorPool m_descriptorPool {VK_NULL_HANDLE};
    VkDescriptorPool m_materialDescriptorPool {VK_NULL_HANDLE};
    std::array<FrameResources, maxFramesInFlight> m_frameResources {};
    TextureSamplers m_textureSamplers;
    TextureImage m_fallbackAlbedo;
    TextureImage m_fallbackNormal;
    VkExtent2D m_fallbackAlbedoExtent {0U, 0U};
    VkExtent2D m_fallbackNormalExtent {0U, 0U};

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
