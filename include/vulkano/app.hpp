#pragma once

#include <cstdint>
#include <memory>
#include <functional>

// External headers (angle brackets per policy)
#include <GLFW/glfw3.h>
#include <vulkano/stats.hpp>

namespace vulkano {

struct AppConfig final {
    const char* title {"Vulkano Codex"};
    std::uint32_t width {1280U};
    std::uint32_t height {720U};
};

class VulkanContext; // fwd decl

class App final {
public:
    explicit App(const AppConfig& config) noexcept;
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;
    ~App() noexcept;

    void run() noexcept; // Minimal loop for scaffolding

private:
    void init_glfw() noexcept;
    void shutdown_glfw() noexcept;
    void init_vulkan() noexcept;
    void shutdown_vulkan() noexcept;

private:
    AppConfig config_ {};
    GLFWwindow* window_ {nullptr};
    std::unique_ptr<VulkanContext> vk_ {};
    bool framebuffer_resized_ {false};
    Stats stats_ {};
    Stats::clock::time_point last_frame_time_ {};

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height) noexcept;
    static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) noexcept;
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) noexcept;
    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) noexcept;
    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) noexcept;

    // UI callback per frame
    void build_ui() noexcept;

    // Input state
    bool look_active_ {false};
    bool first_mouse_ {true};
    bool lock_camera_ {false};
    double last_cursor_x_ {0.0};
    double last_cursor_y_ {0.0};
    bool key_w_ {false};
    bool key_a_ {false};
    bool key_s_ {false};
    bool key_d_ {false};
    bool key_space_ {false};
    bool key_ctrl_ {false};
    bool key_shift_ {false};
};

} // namespace vulkano
