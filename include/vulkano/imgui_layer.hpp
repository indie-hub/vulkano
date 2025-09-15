#pragma once

#include <cstdint>
#include <memory>

namespace vulkano {

class Window;
class VulkanContext;

// Minimal ImGui layer wrapper. Creates ImGui context and Vulkan/GLFW backends.
class ImGuiLayer final {
public:
    ImGuiLayer() = delete;
    ImGuiLayer(const Window& window, VulkanContext& context);
    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;
    ImGuiLayer(ImGuiLayer&&) noexcept;
    ImGuiLayer& operator=(ImGuiLayer&&) noexcept;

    // Begin a new ImGui frame (GLFW + Vulkan backends)
    void begin_frame() const noexcept;
    // Build UI contents for this app; exposes sliders and stats
    void build_ui(std::uint32_t& subdivisions) const noexcept;
    // Called after ImGui::Render to record draw data into given command buffer
    void render(void* cmd_buffer) const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace vulkano

