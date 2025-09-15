#pragma once

#include <vector>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <vulkan_app/camera.hpp>
#include <vulkan_app/renderer.hpp>
#include <vulkan_app/imgui_layer.hpp>
#include <vulkan_app/types.hpp>

namespace vulkan_app {

class Application final {
public:
    Application();
    ~Application() noexcept;
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;

    void run();

private:
    void init_window();
    void main_loop();
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
