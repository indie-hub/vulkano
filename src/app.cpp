#include <vulkano/app.hpp>

#include <chrono>
#include <thread>

namespace vulkano {

namespace {
    constexpr int glfw_context_version_major {3};
    constexpr int glfw_context_version_minor {3};
}

App::App(const AppConfig& config) noexcept : config_ {config} {
    init_glfw();
}

App::~App() noexcept {
    shutdown_glfw();
}

void App::init_glfw() noexcept {
    if (glfwInit() != GLFW_TRUE) {
        // Fail fast: if GLFW cannot init, leave window_ null; run() will exit.
        return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, glfw_context_version_major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, glfw_context_version_minor);

    window_ = glfwCreateWindow(static_cast<int>(config_.width), static_cast<int>(config_.height), config_.title, nullptr, nullptr);
}

void App::shutdown_glfw() noexcept {
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

void App::run() noexcept {
    if (window_ == nullptr) {
        return;
    }

    while (glfwWindowShouldClose(window_) == GLFW_FALSE) {
        glfwPollEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds {16});
    }
}

} // namespace vulkano

