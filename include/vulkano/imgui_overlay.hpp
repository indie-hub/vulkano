#pragma once

#include <cstdint>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

namespace vulkano {

class ImGuiOverlay final {
public:
    ImGuiOverlay() noexcept = default;
    ImGuiOverlay(const ImGuiOverlay&) = delete;
    ImGuiOverlay& operator=(const ImGuiOverlay&) = delete;
    ImGuiOverlay(ImGuiOverlay&&) = delete;
    ImGuiOverlay& operator=(ImGuiOverlay&&) = delete;
    ~ImGuiOverlay() noexcept;

    // Initialise Dear ImGui for GLFW + Vulkan with the given render pass.
    // Requires device/queue to be valid; creates its own descriptor pool.
    void init(GLFWwindow* window,
              VkInstance instance,
              VkPhysicalDevice physicalDevice,
              VkDevice device,
              std::uint32_t graphicsQueueFamily,
              VkQueue graphicsQueue,
              VkRenderPass renderPass,
              std::uint32_t minImageCount) noexcept;

    // Call before issuing draw commands each frame.
    void new_frame() noexcept;

    // Ensure ImGui has built draw data; must be called after building UI for the frame.
    void end_frame_build() noexcept;

    // Record ImGui draw data into the given command buffer. Must be called inside an active render pass.
    void render(VkCommandBuffer cmd) noexcept;

    // Must be called when the render pass changes (e.g., swapchain recreation with new render pass).
    void on_render_pass_changed(VkRenderPass renderPass, std::uint32_t minImageCount) noexcept;

    // Shutdown and free resources.
    void shutdown() noexcept;

private:
    void upload_fonts() noexcept;

private:
    GLFWwindow* window_ {nullptr};
    VkInstance instance_ {VK_NULL_HANDLE};
    VkPhysicalDevice physical_device_ {VK_NULL_HANDLE};
    VkDevice device_ {VK_NULL_HANDLE};
    std::uint32_t graphics_queue_family_ {0U};
    VkQueue graphics_queue_ {VK_NULL_HANDLE};
    VkRenderPass render_pass_ {VK_NULL_HANDLE};
    VkDescriptorPool descriptor_pool_ {VK_NULL_HANDLE};
    std::uint32_t min_image_count_ {2U};
    bool initialized_ {false};
};

} // namespace vulkano

