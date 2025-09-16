#pragma once

#include <cstdint>
#include <vector>
#include <array>

#include <glm/vec2.hpp>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

namespace vulkano {

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

    // Per-frame rendering
    // Returns true if a frame was rendered and queued for present; false if skipped (e.g., out-of-date swapchain).
    bool draw_frame() noexcept;

    // Recreate swapchain and all dependent resources after a resize or format change.
    void recreate_swapchain(GLFWwindow* window) noexcept;

private:
    void create_instance(GLFWwindow* window) noexcept;
    void setup_debug_utils() noexcept;
    void create_surface(GLFWwindow* window) noexcept;
    void pick_physical_device() noexcept;
    void create_logical_device() noexcept;
    void create_swapchain_and_views(GLFWwindow* window) noexcept;
    void create_render_pass() noexcept;
    void create_framebuffers() noexcept;
    void create_pipeline_layout() noexcept;
    void create_graphics_pipeline() noexcept;
    void create_vertex_buffer() noexcept;
    void create_command_pool_and_buffers() noexcept;
    void create_sync_objects() noexcept;
    void record_commands(std::uint32_t imageIndex) noexcept;
    void destroy_swapchain_and_views() noexcept;
    void destroy_framebuffers() noexcept;
    void destroy_pipeline() noexcept;
    void destroy_vertex_buffer() noexcept;
    void destroy_sync_objects() noexcept;
    void destroy_command_pool_and_buffers() noexcept;
    void destroy_render_pass() noexcept;
    void destroy() noexcept;

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

    // Command pool and buffers
    VkCommandPool command_pool_ {VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> command_buffers_ {};

    // Pipeline and vertex buffer
    VkPipelineLayout pipeline_layout_ {VK_NULL_HANDLE};
    VkPipeline graphics_pipeline_ {VK_NULL_HANDLE};
    VkBuffer vertex_buffer_ {VK_NULL_HANDLE};
    VkDeviceMemory vertex_buffer_memory_ {VK_NULL_HANDLE};

    // Synchronisation
    static constexpr std::uint32_t kMaxFramesInFlight {2U};
    std::array<VkSemaphore, kMaxFramesInFlight> image_available_semaphores_ {VK_NULL_HANDLE, VK_NULL_HANDLE};
    std::vector<VkSemaphore> render_finished_semaphores_ {};
    std::array<VkFence, kMaxFramesInFlight> in_flight_fences_ {VK_NULL_HANDLE, VK_NULL_HANDLE};
    std::uint32_t current_frame_ {0U};
};

} // namespace vulkano
