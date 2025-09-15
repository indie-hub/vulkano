#pragma once

#include <vector>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <vulkan_app/camera.hpp>
#include <vulkan_app/renderer.hpp>
#include <vulkan_app/imgui_layer.hpp>
#include <vulkan_app/types.hpp>

namespace vulkan_app {

/**
 * @brief Top-level application that owns the window, renderer, and UI state.
 *
 * Creates GLFW window and Vulkan renderer, integrates ImGui, and drives the
 * main loop. Exposes runtime controls for subdivisions, lighting, material, and
 * SSAO; triggers mesh regeneration when the subdivision level changes.
 */
class Application final {
public:
    /** Construct application and initialize window + renderer. */
    Application();
    /** Destroy application, releasing graphics resources safely. */
    ~Application() noexcept;
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;

    /** Run the main event/render loop. */
    void run();

private:
    /** Create GLFW window and set callbacks. */
    void init_window();
    /** Poll events, update UI, and render each frame. */
    void main_loop();
    /** Rebuild CPU mesh and re-upload GPU buffers if the UI changed. */
    void rebuild_mesh_if_needed();

    GLFWwindow *window{};
    std::unique_ptr<Renderer> renderer{};
    std::unique_ptr<ImGuiLayer> imgui{};
    Camera camera{};
    Light light{};
    Material material{};
    SsaoParams ssao{};
    int subdivisions{2};
    int pending_subdivisions{2};
};

} // namespace vulkan_app
