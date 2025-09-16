#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace vulkano_codex {

struct QueueFamilyIndices final {
    std::optional<uint32_t> graphics {};
    std::optional<uint32_t> present {};
    [[nodiscard]] bool complete() const noexcept {
        return graphics.has_value() && present.has_value();
    }
};

class ImGuiLayer;

class App final {
public:
    App() noexcept = default;
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;

    void run();

private:
    // Window/config
    void* window_ {nullptr}; // GLFWwindow*
    const int initial_width_ {1280};
    const int initial_height_ {720};
    const char* const app_name_ {"VulkanoCodex"};

    // Vulkan core
    VkInstance instance_ {VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT debug_messenger_ {VK_NULL_HANDLE};
    VkSurfaceKHR surface_ {VK_NULL_HANDLE};
    VkPhysicalDevice physical_device_ {VK_NULL_HANDLE};
    VkDevice device_ {VK_NULL_HANDLE};
    VkQueue graphics_queue_ {VK_NULL_HANDLE};
    VkQueue present_queue_ {VK_NULL_HANDLE};
    QueueFamilyIndices queue_indices_ {};

    // Swapchain
    VkSwapchainKHR swapchain_ {VK_NULL_HANDLE};
    VkFormat swapchain_format_ {};
    VkExtent2D swapchain_extent_ {0, 0};
    std::vector<VkImage> swapchain_images_ {};
    std::vector<VkImageView> swapchain_image_views_ {};

    // Pipeline
    VkRenderPass render_pass_ {VK_NULL_HANDLE};
    VkPipelineLayout pipeline_layout_ {VK_NULL_HANDLE};
    VkPipeline pipeline_ {VK_NULL_HANDLE};

    // Framebuffers and commands
    VkCommandPool command_pool_ {VK_NULL_HANDLE};
    std::vector<VkFramebuffer> framebuffers_ {};
    std::vector<VkCommandBuffer> command_buffers_ {};

    // Sync
    static constexpr size_t max_frames_in_flight_ {2};
    std::array<VkSemaphore, max_frames_in_flight_> image_available_ {};
    // Per-frame render-finished semaphores, paired with frames in flight.
    std::array<VkSemaphore, max_frames_in_flight_> render_finished_ {};
    std::array<VkFence, max_frames_in_flight_> in_flight_fences_ {};
    std::vector<VkFence> images_in_flight_ {};
    size_t current_frame_ {0};

    // Vertex buffer
    VkBuffer vertex_buffer_ {VK_NULL_HANDLE};
    VkDeviceMemory vertex_buffer_memory_ {VK_NULL_HANDLE};

    // ImGui
    ImGuiLayer* imgui_ {nullptr};

    // State
    bool framebuffer_resized_ {false};
    std::string device_name_ {};

private:
    void init_window();
    void init_vulkan();
    void create_instance();
    void setup_debug_messenger();
    void create_surface();
    void pick_physical_device();
    void create_logical_device();
    void create_swapchain();
    void create_image_views();
    void create_render_pass();
    void create_pipeline();
    void create_framebuffers();
    void create_command_pool();
    void create_vertex_buffer();
    void create_command_buffers();
    void create_sync_objects();
    void init_imgui();

    void recreate_swapchain();
    void cleanup_swapchain();

    void draw_frame();

    static QueueFamilyIndices find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface);
    static uint32_t find_memory_type(VkPhysicalDevice physical, uint32_t type_filter, VkMemoryPropertyFlags props) noexcept;
};

} // namespace vulkano_codex
