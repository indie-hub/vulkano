#pragma once

#include <cstdint>
#include <memory>

// External headers (angle brackets per policy)
#include <GLFW/glfw3.h>

namespace vulkano {

struct AppConfig final {
    const char* title {"Vulkano Codex"};
    std::uint32_t width {1280U};
    std::uint32_t height {720U};
};

class App final {
public:
    explicit App(const AppConfig& config) noexcept;
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;
    ~App() noexcept;

    void run() noexcept; // Minimal GLFW loop for scaffolding

private:
    void init_glfw() noexcept;
    void shutdown_glfw() noexcept;

private:
    AppConfig config_ {};
    GLFWwindow* window_ {nullptr};
};

} // namespace vulkano

