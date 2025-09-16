#pragma once

#include <cstdint>
#include <memory>
#include <functional>

// External headers (angle brackets per policy)
#include <GLFW/glfw3.h>

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

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height) noexcept;

    // UI callback per frame
    void build_ui() noexcept;
};

} // namespace vulkano
