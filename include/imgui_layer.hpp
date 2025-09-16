#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <string>

class ImGuiLayer {
public:
    ImGuiLayer() = default;
    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    void init(GLFWwindow* window,
              VkInstance instance,
              VkPhysicalDevice physical_device,
              VkDevice device,
              uint32_t graphics_queue_family,
              VkQueue graphics_queue,
              VkDescriptorPool descriptor_pool,
              VkRenderPass render_pass,
              uint32_t image_count);

    void newFrame();
    void render(VkCommandBuffer cmd);

private:
    bool initialized_{false};
};
