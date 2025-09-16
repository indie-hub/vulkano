// Minimal Vulkan application wrapper
#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <array>

struct QueueFamilies {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;
    bool complete() const { return graphics.has_value() && present.has_value(); }
};

struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats{};
    std::vector<VkPresentModeKHR> presentModes{};
};

struct Vertex {
    glm::vec2 pos{};
    static VkVertexInputBindingDescription bindingDescription();
    static std::array<VkVertexInputAttributeDescription, 1> attributeDescriptions();
};

class VulkanApp {
public:
    explicit VulkanApp(GLFWwindow* window);
    ~VulkanApp();

    VulkanApp(const VulkanApp&) = delete;
    VulkanApp& operator=(const VulkanApp&) = delete;

    void drawFrame(const glm::vec4& color);
    void waitIdle() const;
    void notifyFramebufferResized() { framebuffer_resized_ = true; }

    // Stats
    const std::string& deviceName() const { return device_name_; }
    VkExtent2D currentExtent() const { return swapchain_extent_; }

    // ImGui integration helpers
    VkInstance instance() const { return instance_; }
    VkPhysicalDevice physicalDevice() const { return physical_device_; }
    VkDevice device() const { return device_; }
    VkQueue graphicsQueue() const { return graphics_queue_; }
    VkQueue presentQueue() const { return present_queue_; }
    uint32_t graphicsQueueFamily() const { return queue_family_indices_.graphics.value(); }
    VkRenderPass renderPass() const { return render_pass_; }
    VkDescriptorPool imguiDescriptorPool() const { return imgui_descriptor_pool_; }
    uint32_t imageCount() const { return static_cast<uint32_t>(swapchain_images_.size()); }
    VkFormat swapchainImageFormat() const { return swapchain_image_format_; }

    // Resize handling
    void recreateSwapchain();

private:
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createDevice();
    void createSwapchain();
    void createImageViews();
    void createRenderPass();
    void createDescriptorPool();
    void createPipeline();
    void createFramebuffers();
    void createCommandPool();
    void createVertexBuffer();
    void createCommandBuffers();
    void createSyncObjects();

    void cleanupSwapchain();

    QueueFamilies findQueueFamilies(VkPhysicalDevice dev) const;
    SwapchainSupport querySwapchainSupport(VkPhysicalDevice dev) const;
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes) const;
    VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& caps) const;

    VkShaderModule createShaderModule(const std::vector<uint32_t>& code) const;

private:
    GLFWwindow* window_{};

    VkInstance instance_{};
    VkDebugUtilsMessengerEXT debug_messenger_{};
    VkSurfaceKHR surface_{};
    VkPhysicalDevice physical_device_{};
    std::string device_name_{};

    VkDevice device_{};
    QueueFamilies queue_family_indices_{};
    VkQueue graphics_queue_{};
    VkQueue present_queue_{};

    VkSwapchainKHR swapchain_{};
    VkFormat swapchain_image_format_{};
    VkExtent2D swapchain_extent_{};
    std::vector<VkImage> swapchain_images_{};
    std::vector<VkImageView> swapchain_image_views_{};

    VkRenderPass render_pass_{};
    VkPipelineLayout pipeline_layout_{};
    VkPipeline graphics_pipeline_{};

    std::vector<VkFramebuffer> swapchain_framebuffers_{};

    VkCommandPool command_pool_{};
    std::vector<VkCommandBuffer> command_buffers_{};

    VkBuffer vertex_buffer_{};
    VkDeviceMemory vertex_buffer_memory_{};

    VkDescriptorPool imgui_descriptor_pool_{};

    // Synchronization (per-frame-in-flight + per-image)
    static constexpr uint32_t kMaxFramesInFlight{2};
    std::vector<VkSemaphore> image_available_semaphores_{}; // size = kMaxFramesInFlight
    std::vector<VkSemaphore> render_finished_semaphores_{}; // size = kMaxFramesInFlight (image-available)
    std::vector<VkSemaphore> render_finished_image_semaphores_{}; // size = swapchain_images_
    std::vector<VkFence> in_flight_fences_{};               // size = kMaxFramesInFlight
    std::vector<VkFence> images_in_flight_{};               // size = swapchain_images_
    uint32_t current_frame_{0};

    bool framebuffer_resized_{false};
    uint32_t current_image_index_{0};
};
