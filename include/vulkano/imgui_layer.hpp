// imgui_layer.hpp
#pragma once
#include <cstdint>
#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace vulkano {

class ImGuiLayer final {
public:
    ImGuiLayer() = delete;
    ImGuiLayer(GLFWwindow* window, VkInstance instance, VkPhysicalDevice physical_device, VkDevice device, std::uint32_t queue_family_index, VkQueue graphics_queue, VkCommandPool command_pool, VkRenderPass render_pass);
    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;
    ImGuiLayer(ImGuiLayer&&) noexcept;
    ImGuiLayer& operator=(ImGuiLayer&&) noexcept;
    ~ImGuiLayer();

    void new_frame() noexcept;
    void render(void* command_buffer) noexcept; // VkCommandBuffer as void*

private:
    GLFWwindow* m_window {nullptr};
    VkInstance m_instance {nullptr};
    VkDevice m_device {nullptr};
    VkPhysicalDevice m_physical_device {nullptr};
    VkQueue m_graphics_queue {nullptr};
    VkRenderPass m_render_pass {nullptr};
    VkDescriptorPool m_descriptor_pool {nullptr};
    VkCommandPool m_command_pool {nullptr};
    std::uint32_t m_queue_family_index {0U};
};

} // namespace vulkano
