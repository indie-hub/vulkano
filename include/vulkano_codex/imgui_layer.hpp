#pragma once

#include <vulkan/vulkan.h>

namespace vulkano_codex {

class ImGuiLayer final {
public:
    ImGuiLayer() noexcept = default;
    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;
    ImGuiLayer(ImGuiLayer&&) = delete;
    ImGuiLayer& operator=(ImGuiLayer&&) = delete;

    void init(VkInstance instance,
              VkPhysicalDevice physical_device,
              VkDevice device,
              uint32_t queue_family_index,
              VkQueue queue,
              VkRenderPass render_pass,
              uint32_t image_count,
              void* glfw_window);

    void begin_frame() noexcept;
    void end_frame(VkCommandBuffer cmd) noexcept;
    void shutdown(VkDevice device);

private:
    VkDescriptorPool descriptor_pool_ {VK_NULL_HANDLE};
    bool initialized_ {false};
};

} // namespace vulkano_codex
