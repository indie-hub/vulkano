#pragma once

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace vulkan_app {

struct Light;
struct Material;
struct SsaoParams;

class ImGuiLayer final {
public:
    ImGuiLayer(GLFWwindow* window, VkInstance instance, VkDevice device, VkPhysicalDevice gpu, VkQueue graphicsQueue, std::uint32_t graphicsQueueFamily, VkRenderPass renderPass);
    ~ImGuiLayer() noexcept;
    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    void new_frame();
    void draw_ui(int& subdivisions, Light& light, Material& material, SsaoParams& ssao);
    void render(VkCommandBuffer cmd);

private:
    struct Impl;
    Impl* impl_{}; // PIMPL to keep Vulkan objects out of header
};

} // namespace vulkan_app

