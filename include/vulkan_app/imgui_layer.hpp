#pragma once

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace vulkan_app {

struct Light;
struct Material;
struct SsaoParams;

/**
 * @brief Dear ImGui integration using GLFW and Vulkan backends.
 *
 * Wraps backend initialization and frame lifecycle. Provides a single call to
 * build the application UI and a render call that records ImGui draw data into
 * the current command buffer.
 */
class ImGuiLayer final {
public:
    ImGuiLayer(GLFWwindow* window,
               VkInstance instance,
               VkDevice device,
               VkPhysicalDevice gpu,
               VkQueue graphicsQueue,
               std::uint32_t graphicsQueueFamily,
               VkRenderPass renderPass);
    ~ImGuiLayer() noexcept;
    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    /** Start a new ImGui frame. */
    void new_frame();
    /** Build the UI and mutate passed-in parameters based on user input. */
    void draw_ui(int& subdivisions, Light& light, Material& material, SsaoParams& ssao);
    /** Record ImGui draw data into the given command buffer. */
    void render(VkCommandBuffer cmd);

private:
    struct Impl;
    Impl* impl_{}; // PIMPL to keep Vulkan objects out of header
};

} // namespace vulkan_app
