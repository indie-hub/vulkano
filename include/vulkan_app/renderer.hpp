#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace vulkan_app {

struct Vertex;
struct Mesh;
struct CameraUBO;
struct Light;
struct Material;
struct SsaoParams;
class ImGuiLayer;

/**
 * @brief Vulkan renderer that manages instance, device, swapchain, and drawing.
 *
 * Provides a minimal public surface for the application to update geometry and
 * per-frame parameters, and exposes required handles for ImGui backend
 * initialization.
 */
class Renderer final {
public:
    /**
     * Initialize Vulkan instance, device, swapchain, render pass, pipeline(s), and static resources.
     * @param window GLFW window used to create the presentation surface.
     */
    explicit Renderer(GLFWwindow* window);
    /** Destroy Vulkan resources in a safe order. */
    ~Renderer() noexcept;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    /** Trigger swapchain recreation on resize. */
    void resize();
    /** Upload new mesh vertex/index buffers to the GPU. */
    void update_mesh(const Mesh& mesh);
    /**
     * Record and submit rendering for the current frame.
     * @param camera Camera UBO data (view/proj matrices and camera position).
     * @param light Light parameters for Phong shading.
     * @param material Material parameters (albedo and normal map strength).
     * @param ssao Parameters controlling SSAO kernel, radius, bias, and blur.
     */
    void draw_frame(const CameraUBO& camera, const Light& light, const Material& material, const SsaoParams& ssao);
    [[nodiscard]] const char* device_name() const noexcept { return device_name_; }
    void attach_imgui(ImGuiLayer* layer) noexcept { imgui_layer_ = layer; }

    // Minimal accessors for integration points
    [[nodiscard]] VkInstance instance() const noexcept { return instance_; }
    [[nodiscard]] VkDevice device() const noexcept { return device_; }
    [[nodiscard]] VkPhysicalDevice physical_device() const noexcept { return physical_device_; }
    [[nodiscard]] VkQueue graphics_queue() const noexcept { return graphics_queue_; }
    [[nodiscard]] std::uint32_t graphics_queue_family() const noexcept { return graphics_family_; }
    [[nodiscard]] VkRenderPass render_pass() const noexcept { return render_pass_; }
    [[nodiscard]] VkExtent2D swapchain_extent() const noexcept { return swapchain_extent_; }

private:
    void init_instance();
    void init_surface(GLFWwindow* window);
    void pick_physical_device();
    void create_device();
    void create_swapchain();
    void destroy_swapchain();
    void create_render_pass();
    void create_gbuffer_pass();
    void create_pipeline();
    void create_gbuffer_pipeline();
    void create_compose_pipeline();
    void create_framebuffers();
    void create_gbuffer_targets();
    void create_ao_targets();
    void create_command_pool_and_buffers();
    void create_sync_objects();
    void create_descriptor_layouts();
    void create_descriptor_pool_and_sets();
    void create_depth_resources();
    void create_sampler();
    void create_textures_if_needed();
    void recreate_swapchain();
    void create_compose_descriptors();
    void create_ssao_pipeline();
    void create_ssao_descriptors();
    void create_blur_pipeline();
    void create_blur_descriptors();

    VkInstance instance_{};
    VkDebugUtilsMessengerEXT debug_messenger_{};
    VkSurfaceKHR surface_{};
    VkPhysicalDevice physical_device_{};
    VkDevice device_{};
    VkQueue graphics_queue_{};
    VkQueue present_queue_{};
    std::uint32_t graphics_family_{0U};
    std::uint32_t present_family_{0U};
    VkSwapchainKHR swapchain_{};
    VkFormat swapchain_format_{};
    VkExtent2D swapchain_extent_{};
    const char* device_name_{""};

    // swapchain
    std::vector<VkImage> swapchain_images_{};
    std::vector<VkImageView> swapchain_image_views_{};

    // render targets
    VkRenderPass render_pass_{};
    std::vector<VkFramebuffer> framebuffers_{};
    VkImage depth_image_{};
    VkDeviceMemory depth_memory_{};
    VkImageView depth_view_{};
    VkFormat depth_format_{};

    // G-buffer offscreen
    VkRenderPass gbuffer_pass_{};
    VkFramebuffer gbuffer_fb_{};
    VkImage g_scene_image_{}; VkDeviceMemory g_scene_mem_{}; VkImageView g_scene_view_{};
    VkImage g_normal_image_{}; VkDeviceMemory g_normal_mem_{}; VkImageView g_normal_view_{};
    VkImage g_viewz_image_{}; VkDeviceMemory g_viewz_mem_{}; VkImageView g_viewz_view_{};
    VkImage g_depth_image_{}; VkDeviceMemory g_depth_mem_{}; VkImageView g_depth_view_{};

    // SSAO resources
    VkRenderPass ssao_pass_{};
    VkFramebuffer ssao_fb_raw_{};
    VkFramebuffer ssao_fb_blur_{};
    VkImage ao_image_{}; VkDeviceMemory ao_memory_{}; VkImageView ao_view_{};
    VkImage ao_blur_image_{}; VkDeviceMemory ao_blur_memory_{}; VkImageView ao_blur_view_{};
    VkImage noise_image_{}; VkDeviceMemory noise_memory_{}; VkImageView noise_view_{};
    VkBuffer ssao_params_buffer_{}; VkDeviceMemory ssao_params_memory_{};
    VkBuffer ssao_kernel_buffer_{}; VkDeviceMemory ssao_kernel_memory_{};
    VkDescriptorSetLayout ssao_set_layout_{}; VkDescriptorPool ssao_pool_{}; VkDescriptorSet ssao_set_{};
    VkPipelineLayout ssao_pipeline_layout_{}; VkPipeline ssao_pipeline_{};

    // Blur resources
    VkDescriptorSetLayout blur_set_layout_{}; VkDescriptorPool blur_pool_{}; VkDescriptorSet blur_set_input_raw_{}; VkDescriptorSet blur_set_input_blur_{};
    VkPipelineLayout blur_pipeline_layout_{}; VkPipeline blur_pipeline_{};

    // pipeline
    VkPipelineLayout pipeline_layout_{};
    VkPipeline pipeline_{};
    VkPipelineLayout gbuffer_pipeline_layout_{};
    VkPipeline gbuffer_pipeline_{};
    VkPipelineLayout compose_pipeline_layout_{};
    VkPipeline compose_pipeline_{};

    // descriptors
    VkDescriptorSetLayout desc_set_layout_{};
    VkDescriptorPool desc_pool_{};
    std::vector<VkDescriptorSet> desc_sets_{};
    // Compose descriptors
    VkDescriptorSetLayout compose_set_layout_{};
    VkDescriptorPool compose_pool_{};
    std::vector<VkDescriptorSet> compose_sets_{};

    // resources
    VkBuffer vertex_buffer_{};
    VkDeviceMemory vertex_memory_{};
    VkBuffer index_buffer_{};
    VkDeviceMemory index_memory_{};
    std::uint32_t index_count_{0U};

    // textures
    VkImage albedo_image_{};
    VkDeviceMemory albedo_memory_{};
    VkImageView albedo_view_{};
    VkImage normal_image_{};
    VkDeviceMemory normal_memory_{};
    VkImageView normal_view_{};
    VkSampler sampler_{};

    // per-frame
    VkCommandPool cmd_pool_{};
    std::vector<VkCommandBuffer> cmd_buffers_{};
    std::vector<VkSemaphore> image_available_{};
    std::vector<VkSemaphore> render_finished_{};
    std::vector<VkFence> in_flight_{};
    std::size_t current_frame_{0U};

    // UBOs
    VkBuffer camera_buffer_{}; VkDeviceMemory camera_memory_{};
    VkBuffer material_buffer_{}; VkDeviceMemory material_memory_{};
    VkBuffer light_buffer_{}; VkDeviceMemory light_memory_{};

    // ImGui integration (optional)
    ImGuiLayer* imgui_layer_{};
};

} // namespace vulkan_app
