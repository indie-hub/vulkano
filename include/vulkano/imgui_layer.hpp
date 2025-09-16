// imgui_layer.hpp
#pragma once
#include <cstdint>

typedef struct VkInstance_T* VkInstance;
typedef struct VkDevice_T* VkDevice;
typedef struct VkRenderPass_T* VkRenderPass;
typedef struct VkDescriptorPool_T* VkDescriptorPool;
typedef struct VkQueue_T* VkQueue;

struct GLFWwindow;

namespace vulkano {

class ImGuiLayer final {
public:
    ImGuiLayer() = delete;
    ImGuiLayer(GLFWwindow* window, VkInstance instance, VkDevice device, VkQueue graphics_queue, VkRenderPass render_pass);
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
    VkQueue m_graphics_queue {nullptr};
    VkRenderPass m_render_pass {nullptr};
    VkDescriptorPool m_descriptor_pool {nullptr};
};

} // namespace vulkano

