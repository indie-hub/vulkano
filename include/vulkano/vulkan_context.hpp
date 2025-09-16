// vulkan_context.hpp
#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

typedef struct VkInstance_T* VkInstance;
typedef struct VkSurfaceKHR_T* VkSurfaceKHR;
typedef struct VkDevice_T* VkDevice;
typedef struct VkRenderPass_T* VkRenderPass;
typedef struct VkPipelineLayout_T* VkPipelineLayout;
typedef struct VkPipeline_T* VkPipeline;
typedef struct VkSwapchainKHR_T* VkSwapchainKHR;
typedef struct VkCommandPool_T* VkCommandPool;
typedef struct VkBuffer_T* VkBuffer;
typedef struct VkDeviceMemory_T* VkDeviceMemory;
typedef struct VkImageView_T* VkImageView;
typedef struct VkFramebuffer_T* VkFramebuffer;
typedef struct VkSemaphore_T* VkSemaphore;
typedef struct VkFence_T* VkFence;
typedef struct VkQueue_T* VkQueue;
typedef struct VkPhysicalDevice_T* VkPhysicalDevice;

struct GLFWwindow;

namespace vulkano {

struct SwapchainExtent final {
    std::uint32_t width {0U};
    std::uint32_t height {0U};
};

struct Vertex final {
    glm::vec2 pos {};
};

struct PushConstants final {
    glm::vec4 color {1.0F, 1.0F, 1.0F, 1.0F};
};

class VulkanContext final {
public:
    VulkanContext() = delete;
    VulkanContext(GLFWwindow* window, std::uint32_t width, std::uint32_t height);
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&) noexcept;
    VulkanContext& operator=(VulkanContext&&) noexcept;
    ~VulkanContext();

    void draw_frame(const glm::vec4& color, bool& swapchain_recreated, void (*imgui_render)(void* cmd_buf, void* user) = nullptr, void* user_ptr = nullptr);
    void wait_idle() const noexcept;
    void recreate_swapchain(std::uint32_t width, std::uint32_t height);

    [[nodiscard]] const char* device_name() const noexcept;
    [[nodiscard]] SwapchainExtent current_extent() const noexcept;
    [[nodiscard]] VkDevice device() const noexcept;
    [[nodiscard]] VkRenderPass render_pass() const noexcept;
    [[nodiscard]] VkInstance instance() const noexcept;
    [[nodiscard]] void* graphics_queue() const noexcept; // VkQueue as void*
    [[nodiscard]] void* command_pool() const noexcept;   // VkCommandPool as void*
    [[nodiscard]] void* physical_device() const noexcept; // VkPhysicalDevice as void*
    [[nodiscard]] std::uint32_t graphics_family_index() const noexcept;

private:
    void create_instance();
    void setup_debug();
    void create_surface(GLFWwindow* window);
    void pick_physical_device();
    void create_logical_device();
    void create_swapchain(std::uint32_t width, std::uint32_t height);
    void create_image_views();
    void create_render_pass();
    void create_graphics_pipeline();
    void create_framebuffers();
    void create_command_pool();
    void create_vertex_buffer();
    void create_command_buffers();
    void create_sync_objects();
    void cleanup_swapchain();

private:
    VkInstance m_instance {nullptr};
    void* m_debug_messenger {nullptr}; // VkDebugUtilsMessengerEXT stored opaquely
    VkSurfaceKHR m_surface {nullptr};
    VkPhysicalDevice m_physical_device {nullptr};
    VkDevice m_device {nullptr};
    VkQueue m_graphics_queue {nullptr};
    VkQueue m_present_queue {nullptr};
    std::uint32_t m_graphics_family_index {0U};
    VkSwapchainKHR m_swapchain {nullptr};
    std::vector<VkImageView> m_swapchain_image_views {};
    VkRenderPass m_render_pass {nullptr};
    VkPipelineLayout m_pipeline_layout {nullptr};
    VkPipeline m_pipeline {nullptr};
    std::vector<VkFramebuffer> m_framebuffers {};
    VkCommandPool m_command_pool {nullptr};
    std::vector<void*> m_command_buffers {};
    VkBuffer m_vertex_buffer {nullptr};
    VkDeviceMemory m_vertex_memory {nullptr};
    std::vector<VkSemaphore> m_image_available {};
    std::vector<VkSemaphore> m_render_finished {};
    std::vector<VkFence> m_in_flight_fences {};
    std::size_t m_current_frame {0U};
    SwapchainExtent m_extent {};
    std::string m_device_name {};
    VkFormat m_swapchain_format {VK_FORMAT_B8G8R8A8_SRGB};
};

} // namespace vulkano
