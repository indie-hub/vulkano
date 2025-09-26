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
#include <vulkano/texture.hpp>
#include <vulkano/texture_types.hpp>
#include <vulkano/vulkan_context.hpp>
#include <vulkano/window.hpp>

#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/constants.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include <chrono>
#include <string>

#include <imgui.h>

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
    [[nodiscard]] auto ssao_enabled() const noexcept -> bool;
    [[nodiscard]] auto ssao_sample_count() const noexcept -> std::uint32_t;
    [[nodiscard]] auto ssao_extent() const noexcept -> std::array<std::uint32_t, 2U>;
    [[nodiscard]] auto ssao_noise_size() const noexcept -> std::uint32_t;
    [[nodiscard]] auto ssao_base_radius() const noexcept -> float;
    [[nodiscard]] auto ssao_bias() const noexcept -> float;

private:
    struct ScenePrimitive;

    static constexpr std::size_t maxCascades {4U};

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
    void destroy_shadow_debug_textures() noexcept;
    void update_shadow_debug_textures();
    void ensure_shadow_map_read_layout(VkCommandBuffer commandBuffer);
    void recreate_shadow_resources();
    void initialise_scene();
    void rebuild_dirty_meshes();
    void create_descriptor_resources();
    void destroy_descriptor_resources() noexcept;
    void destroy_material_descriptor_resources(bool destroyLayout) noexcept;
    void ensure_descriptor_layouts();
    void create_fallback_textures();
    void destroy_fallback_textures() noexcept;
    void ensure_material_descriptor(ScenePrimitive& primitive);
    void update_material_descriptor(ScenePrimitive& primitive);
    [[nodiscard]] auto fallback_normal_texture(NormalMapStyle style) const noexcept -> TextureHandle;
    void create_ssao_resources();
    void destroy_ssao_resources() noexcept;
    void recreate_ssao_resources();
    void create_ssao_descriptor_resources();
    void destroy_ssao_descriptor_resources() noexcept;
    void generate_ssao_kernel();
    void update_ssao_settings_buffer();
    void ensure_ssao_noise_texture();
    void update_ssao_descriptors();
    void rebuild_global_descriptors();
    void record_depth_prepass(VkCommandBuffer commandBuffer, std::uint32_t imageIndex);
    void record_ssao_pass(VkCommandBuffer commandBuffer, std::uint32_t imageIndex);
    void record_ssao_blur_pass(VkCommandBuffer commandBuffer, std::uint32_t imageIndex);
    void transition_image_layout(
        VkCommandBuffer commandBuffer,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkPipelineStageFlags srcStage,
        VkPipelineStageFlags dstStage,
        VkAccessFlags srcAccess,
        VkAccessFlags dstAccess,
        VkImageAspectFlags aspectMask) const;
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
        TextureHandle albedoTexture {};
        TextureHandle normalTexture {};
        VkDescriptorSet materialDescriptor {VK_NULL_HANDLE};
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

    struct ShadowSettings final {
        bool enabled {true};
        float depthBiasConstant {1.25F};
        float depthBiasSlope {1.75F};
        float minBias {0.0025F};
        float normalBiasFactor {0.05F};
        int pcfRadius {1};
        std::uint32_t cascadeCount {3U};
        float splitLambda {0.5F};
        std::uint32_t resolution {ShadowMap::defaultResolution};
    };

    struct SsaoSettings final {
        bool enabled {true};
        float radius {0.5F};
        float bias {0.025F};
        float power {1.5F};
        float intensity {1.0F};
        float blurSigma {2.0F};
        std::uint32_t sampleCount {32U};
        bool halfResolution {false};
    };

    SceneState m_scene {};
    CameraState m_camera {};
    DepthResources m_depthResources {};
    ShadowMap m_shadowMap {};
    ShadowRenderPass m_shadowRenderPass {};
    ShadowPipeline m_shadowPipeline {};
    std::vector<VkFramebuffer> m_shadowFramebuffers {};
    ShadowSettings m_shadowSettings {};
    SsaoSettings m_ssaoSettings {};
    std::array<glm::mat4, maxCascades> m_cascadeMatrices {};
    std::array<float, maxCascades> m_cascadeSplits {};
    std::uint32_t m_activeCascadeCount {1U};
    bool m_shadowResourcesDirty {false};
    std::vector<ImTextureID> m_shadowDebugTextures {};
    VkImageLayout m_shadowImageLayout {VK_IMAGE_LAYOUT_UNDEFINED};
    VkFormat m_depthFormat {VK_FORMAT_D32_SFLOAT};
    VkDescriptorSetLayout m_descriptorSetLayout {VK_NULL_HANDLE};
    VkDescriptorSetLayout m_materialDescriptorSetLayout {VK_NULL_HANDLE};
    VkDescriptorPool m_descriptorPool {VK_NULL_HANDLE};
    VkDescriptorPool m_materialDescriptorPool {VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> m_descriptorSets {};
    Buffer m_globalUniformBuffer;
    TextureHandle m_fallbackAlbedoTexture {};
    TextureHandle m_fallbackNormalNoiseTexture {};
    TextureHandle m_fallbackNormalMetalTexture {};
    VkRenderPass m_depthPrepassRenderPass {VK_NULL_HANDLE};
    VkPipelineLayout m_depthPrepassPipelineLayout {VK_NULL_HANDLE};
    VkPipeline m_depthPrepassPipeline {VK_NULL_HANDLE};
    std::vector<VkFramebuffer> m_depthPrepassFramebuffers {};
    VkPipelineLayout m_ssaoPipelineLayout {VK_NULL_HANDLE};
    VkPipeline m_ssaoPipeline {VK_NULL_HANDLE};
    VkPipelineLayout m_ssaoBlurPipelineLayout {VK_NULL_HANDLE};
    VkPipeline m_ssaoBlurPipeline {VK_NULL_HANDLE};

    VkDescriptorSetLayout m_ssaoDescriptorSetLayout {VK_NULL_HANDLE};
    VkDescriptorSetLayout m_ssaoBlurDescriptorSetLayout {VK_NULL_HANDLE};
    VkDescriptorPool m_ssaoDescriptorPool {VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> m_ssaoDescriptorSets {};
    std::vector<VkDescriptorSet> m_ssaoBlurDescriptorSets {};
    std::vector<VkImage> m_gbufferNormalImages {};
    std::vector<VkDeviceMemory> m_gbufferNormalMemories {};
    std::vector<VkImageView> m_gbufferNormalViews {};
    std::vector<VkImage> m_gbufferPositionImages {};
    std::vector<VkDeviceMemory> m_gbufferPositionMemories {};
    std::vector<VkImageView> m_gbufferPositionViews {};
    std::vector<VkImage> m_ssaoOcclusionImages {};
    std::vector<VkImage> m_ssaoBlurImages {};
    std::vector<VkDeviceMemory> m_ssaoOcclusionMemories {};
    std::vector<VkDeviceMemory> m_ssaoBlurMemories {};
    std::vector<VkImageView> m_ssaoOcclusionViews {};
    std::vector<VkImageView> m_ssaoBlurViews {};
    std::vector<VkImageLayout> m_ssaoBlurLayouts {};
    std::vector<VkImageLayout> m_ssaoOcclusionLayouts {};
    VkSampler m_ssaoSampler {VK_NULL_HANDLE};
    TextureHandle m_ssaoNoiseTexture {};
    Buffer m_ssaoKernelBuffer;
    Buffer m_ssaoSettingsBuffer;
    std::vector<glm::vec4> m_ssaoKernel {};
    VkExtent2D m_ssaoExtent {0U, 0U};
    bool m_ssaoResourcesDirty {false};

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
