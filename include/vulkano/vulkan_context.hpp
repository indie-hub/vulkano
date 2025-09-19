#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <array>
#include <string>
#include <memory>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>


#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

namespace vulkano {

class ImGuiOverlay;
class Camera;
class Primitive;

class VulkanContext final {
public:
    explicit VulkanContext(GLFWwindow* window) noexcept;
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&) = delete;
    VulkanContext& operator=(VulkanContext&&) = delete;
    ~VulkanContext() noexcept;

    [[nodiscard]] VkInstance instance() const noexcept;
    [[nodiscard]] VkSurfaceKHR surface() const noexcept;
    [[nodiscard]] bool validation_enabled() const noexcept;

    // Device accessors
    [[nodiscard]] VkPhysicalDevice physical_device() const noexcept;
    [[nodiscard]] VkDevice device() const noexcept;
    [[nodiscard]] VkQueue graphics_queue() const noexcept;
    [[nodiscard]] VkQueue present_queue() const noexcept;
    [[nodiscard]] std::uint32_t graphics_queue_family() const noexcept;
    [[nodiscard]] std::uint32_t present_queue_family() const noexcept;

    // Swapchain accessors
    [[nodiscard]] VkFormat swapchain_image_format() const noexcept;
    [[nodiscard]] VkExtent2D swapchain_extent() const noexcept;
    [[nodiscard]] const std::string& device_name() const noexcept;
    [[nodiscard]] bool sampler_anisotropy_supported() const noexcept;
    [[nodiscard]] float max_sampler_anisotropy() const noexcept;

    // Texture info (for UI display)
    [[nodiscard]] std::uint32_t albedo_width() const noexcept;
    [[nodiscard]] std::uint32_t albedo_height() const noexcept;
    [[nodiscard]] std::uint32_t albedo_mip_levels() const noexcept;
    [[nodiscard]] std::uint32_t normal_width() const noexcept;
    [[nodiscard]] std::uint32_t normal_height() const noexcept;
    [[nodiscard]] std::uint32_t normal_mip_levels() const noexcept;
    [[nodiscard]] VkFormat albedo_format() const noexcept;
    [[nodiscard]] VkFormat normal_format() const noexcept;
    [[nodiscard]] const std::string& albedo_label() const noexcept;
    [[nodiscard]] const std::string& normal_label() const noexcept;
    // Human-readable format strings for UI
    [[nodiscard]] std::string albedo_format_string() const noexcept;
    [[nodiscard]] std::string normal_format_string() const noexcept;

    // Per-frame rendering
    // Returns true if a frame was rendered and queued for present; false if skipped (e.g., out-of-date swapchain).
    bool draw_frame() noexcept;

    // ImGui per-frame hooks
    void imgui_new_frame() noexcept;
    // End current ImGui frame build without rendering (used when skipping a frame)
    void imgui_end_frame_build() noexcept;

    // Recreate swapchain and all dependent resources after a resize or format change.
    void recreate_swapchain(GLFWwindow* window) noexcept;

    // Scene parameters (light). UI will tweak these later.
    struct Light final {
        glm::vec3 position {2.0F, 4.0F, 2.0F};
        float intensity {1.0F};
        glm::vec3 color {1.0F, 1.0F, 1.0F};
        float ambient {0.1F};
    };
    [[nodiscard]] const Light& light() const noexcept;
    void set_light(const Light& l) noexcept;

    // Camera interaction (for input handling)
    void camera_orbit_delta(float dYaw, float dPitch) noexcept;
    void camera_zoom_delta(float dDistance) noexcept;
    void camera_set_aspect(float aspect) noexcept;
    void camera_fov_delta(float dFovRadians) noexcept;
    // FPS-style helpers
    void camera_look_delta(float dYaw, float dPitch) noexcept;
    void camera_move_local(const glm::vec3& deltaLocal) noexcept;

    // Camera readbacks for UI
    [[nodiscard]] glm::vec3 camera_position() const noexcept;
    [[nodiscard]] glm::vec2 camera_angles() const noexcept; // x=yaw, y=pitch
    [[nodiscard]] float camera_fov() const noexcept;

    // SSAO parameters and toggles (UI-controlled)
    struct SsaoParams final {
        bool enabled {false};
        std::int32_t kernelSize {32};
        float radius {0.5F};
        float bias {0.025F};
        float power {1.0F};
        bool blur_enabled {true};
        float blur_radius {2.0F};
        float blur_sigma {1.0F};
        float strength {1.0F};
    };
    [[nodiscard]] const SsaoParams& ssao_params() const noexcept;
    void set_ssao_params(const SsaoParams& p) noexcept;

    // Scene editing helpers
    [[nodiscard]] std::size_t primitive_count() const noexcept;
    [[nodiscard]] Primitive* primitive_at(std::size_t index) noexcept;
    [[nodiscard]] const Primitive* primitive_at(std::size_t index) const noexcept;
    void rebuild_scene_gpu_buffers() noexcept;

private:
    void create_instance(GLFWwindow* window) noexcept;
    void setup_debug_utils() noexcept;
    void create_surface(GLFWwindow* window) noexcept;
    void pick_physical_device() noexcept;
    void create_logical_device() noexcept;
    void create_swapchain_and_views(GLFWwindow* window) noexcept;
    void create_render_pass() noexcept;
    void create_framebuffers() noexcept;
    void create_depth_resources() noexcept;
    void destroy_depth_resources() noexcept;
    void create_pipeline_layout() noexcept;
    void create_graphics_pipeline() noexcept;
    void create_vertex_buffer() noexcept; // legacy triangle fallback
    void create_scene() noexcept;
    void create_scene_buffers() noexcept;
    void create_default_textures() noexcept;
    void destroy_textures() noexcept;
    // Buffer helpers (staging uploads)
    bool create_buffer(VkDeviceSize size,
                       VkBufferUsageFlags usage,
                       VkMemoryPropertyFlags properties,
                       VkBuffer& buffer,
                       VkDeviceMemory& memory,
                       const char* debugName) noexcept;
    bool copy_buffer(VkBuffer src,
                     VkBuffer dst,
                     VkDeviceSize size) noexcept;
    // Image helpers
    bool create_image_2d(std::uint32_t width,
                         std::uint32_t height,
                         std::uint32_t mipLevels,
                         VkFormat format,
                         VkImageTiling tiling,
                         VkImageUsageFlags usage,
                         VkMemoryPropertyFlags properties,
                         VkImage& image,
                         VkDeviceMemory& memory,
                         const char* debugName) noexcept;
    bool create_image_view_2d(VkImage image,
                              VkFormat format,
                              VkImageAspectFlags aspect,
                              std::uint32_t mipLevels,
                              VkImageView& view,
                              const char* debugName) noexcept;
    void transition_image_layout(VkImage image,
                                 VkFormat format,
                                 VkImageLayout oldLayout,
                                 VkImageLayout newLayout,
                                 std::uint32_t mipLevels) noexcept;
    bool copy_buffer_to_image(VkBuffer buffer,
                              VkImage image,
                              std::uint32_t width,
                              std::uint32_t height) noexcept;
    void generate_mipmaps(VkImage image,
                          VkFormat imageFormat,
                          std::uint32_t texWidth,
                          std::uint32_t texHeight,
                          std::uint32_t mipLevels) noexcept;
    void create_descriptor_set_layout() noexcept;
    void create_uniform_buffers_and_sets() noexcept;
    void create_command_pool_and_buffers() noexcept;
    void create_sync_objects() noexcept;
    // Returns true if commands were recorded successfully for the image.
    bool record_commands(std::uint32_t imageIndex) noexcept;
    void init_imgui(GLFWwindow* window) noexcept;
    void destroy_swapchain_and_views() noexcept;
    void destroy_framebuffers() noexcept;
    void destroy_pipeline() noexcept;
    void destroy_vertex_buffer() noexcept;
    void destroy_index_buffer() noexcept;
    void destroy_descriptor_set_layout() noexcept;
    void destroy_uniform_buffers_and_sets() noexcept;
    void destroy_sync_objects() noexcept;
    void destroy_command_pool_and_buffers() noexcept;
    void destroy_render_pass() noexcept;
    void destroy() noexcept;

    // Debug utils helpers
    void set_object_name(VkObjectType type, std::uint64_t handle, const char* name) const noexcept;
    void begin_label(VkCommandBuffer cmd, const char* name) const noexcept;
    void end_label(VkCommandBuffer cmd) const noexcept;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) noexcept;

private:
    VkInstance instance_ {VK_NULL_HANDLE};
    VkSurfaceKHR surface_ {VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT debug_messenger_ {VK_NULL_HANDLE};
    bool validation_enabled_ {false};

    // Device and queues
    VkPhysicalDevice physical_device_ {VK_NULL_HANDLE};
    VkDevice device_ {VK_NULL_HANDLE};
    VkQueue graphics_queue_ {VK_NULL_HANDLE};
    VkQueue present_queue_ {VK_NULL_HANDLE};
    std::uint32_t graphics_family_index_ {UINT32_MAX};
    std::uint32_t present_family_index_ {UINT32_MAX};

    // Swapchain
    VkSwapchainKHR swapchain_ {VK_NULL_HANDLE};
    VkFormat swapchain_image_format_ {VK_FORMAT_UNDEFINED};
    VkExtent2D swapchain_extent_ {0U, 0U};
    std::vector<VkImage> swapchain_images_ {};
    std::vector<VkImageView> swapchain_image_views_ {};
    std::vector<VkFence> images_in_flight_ {};

    // Render pass and framebuffers
    VkRenderPass render_pass_ {VK_NULL_HANDLE};
    std::vector<VkFramebuffer> framebuffers_ {};
    // Depth resources
    VkImage depth_image_ {VK_NULL_HANDLE};
    VkDeviceMemory depth_image_memory_ {VK_NULL_HANDLE};
    VkImageView depth_image_view_ {VK_NULL_HANDLE};
    VkFormat depth_format_ {VK_FORMAT_UNDEFINED};

    // Command pool and buffers
    VkCommandPool command_pool_ {VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> command_buffers_ {};

    // Pipeline and mesh buffers
    VkPipelineLayout pipeline_layout_ {VK_NULL_HANDLE};
    VkPipeline graphics_pipeline_ {VK_NULL_HANDLE};
    VkBuffer vertex_buffer_ {VK_NULL_HANDLE};
    VkDeviceMemory vertex_buffer_memory_ {VK_NULL_HANDLE};
    VkBuffer index_buffer_ {VK_NULL_HANDLE};
    VkDeviceMemory index_buffer_memory_ {VK_NULL_HANDLE};
    // Descriptor set layout/pool for global UBO
    VkDescriptorSetLayout descriptor_set_layout_ {VK_NULL_HANDLE};
    VkDescriptorPool descriptor_pool_ {VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> descriptor_sets_ {};
    std::vector<VkBuffer> uniform_buffers_ {};
    std::vector<VkDeviceMemory> uniform_buffers_memory_ {};
    // Textures (global defaults)
    VkImage albedo_image_ {VK_NULL_HANDLE};
    VkDeviceMemory albedo_image_memory_ {VK_NULL_HANDLE};
    VkImageView albedo_image_view_ {VK_NULL_HANDLE};
    VkSampler albedo_sampler_ {VK_NULL_HANDLE};
    std::uint32_t albedo_width_ {0U};
    std::uint32_t albedo_height_ {0U};
    std::uint32_t albedo_mip_levels_ {1U};
    VkFormat albedo_format_ {VK_FORMAT_UNDEFINED};
    std::string albedo_label_ {};

    VkImage normal_image_ {VK_NULL_HANDLE};
    VkDeviceMemory normal_image_memory_ {VK_NULL_HANDLE};
    VkImageView normal_image_view_ {VK_NULL_HANDLE};
    VkSampler normal_sampler_ {VK_NULL_HANDLE};
    std::uint32_t normal_width_ {0U};
    std::uint32_t normal_height_ {0U};
    std::uint32_t normal_mip_levels_ {1U};
    VkFormat normal_format_ {VK_FORMAT_UNDEFINED};
    std::string normal_label_ {};

    // Synchronisation
    static constexpr std::uint32_t kMaxFramesInFlight {2U};
    std::array<VkSemaphore, kMaxFramesInFlight> image_available_semaphores_ {VK_NULL_HANDLE, VK_NULL_HANDLE};
    std::vector<VkSemaphore> render_finished_semaphores_ {};
    std::array<VkFence, kMaxFramesInFlight> in_flight_fences_ {VK_NULL_HANDLE, VK_NULL_HANDLE};
    std::uint32_t current_frame_ {0U};

    // ImGui
    std::unique_ptr<ImGuiOverlay> imgui_ {};
    bool imgui_ready_ {false};
    bool imgui_frame_started_ {false};
    std::string device_name_cached_ {};
    // Device capabilities cached for UI/texture setup
    bool sampler_anisotropy_supported_ {false};
    float max_sampler_anisotropy_ {1.0F};

    // Camera
    std::unique_ptr<Camera> camera_ {};
    Light light_ {};

    // Scene: CPU primitives and GPU draw ranges
    std::vector<std::unique_ptr<Primitive>> primitives_ {};
    struct DrawRange final {
        std::uint32_t firstIndex {0U};
        std::uint32_t indexCount {0U};
        std::uint32_t firstVertex {0U};
        Primitive* prim {nullptr}; // non-owning
    };
    std::vector<DrawRange> draw_ranges_ {};

    // Debug utils function pointers (device level)
    PFN_vkSetDebugUtilsObjectNameEXT pfn_set_name_ {nullptr};
    PFN_vkCmdBeginDebugUtilsLabelEXT pfn_cmd_begin_label_ {nullptr};
    PFN_vkCmdEndDebugUtilsLabelEXT pfn_cmd_end_label_ {nullptr};
    // Reserved for future per-frame UI callback

    // SSAO runtime parameters (resources will be created in subsequent steps)
    SsaoParams ssao_ {};
};

} // namespace vulkano
