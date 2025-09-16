#include <vulkano/app.hpp>
#include <vulkano/vulkan_context.hpp>

#include <chrono>
#include <cstdlib>
#include <thread>

namespace vulkano {

namespace {
    constexpr int glfw_context_version_major {3};
    constexpr int glfw_context_version_minor {3};
}

App::App(const AppConfig& config) noexcept : config_ {config} {
    init_glfw();
    init_vulkan();
}

App::~App() noexcept {
    shutdown_vulkan();
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

void App::init_vulkan() noexcept {
    if (window_ == nullptr) {
        return;
    }
    vk_ = std::make_unique<VulkanContext>(window_);
}

void App::shutdown_vulkan() noexcept {
    vk_.reset();
}

void App::run() noexcept {
    if (window_ == nullptr) {
        return;
    }

    const char* envAutoClose {std::getenv("VK_APP_AUTOCLOSE_MS")};
    const bool useAutoClose {envAutoClose != nullptr};
    const std::chrono::milliseconds autoCloseMs {
        useAutoClose ? std::chrono::milliseconds {std::strtol(envAutoClose, nullptr, 10)} : std::chrono::milliseconds {0}
    };
    const auto startTime {std::chrono::steady_clock::now()};

    while (glfwWindowShouldClose(window_) == GLFW_FALSE) {
        glfwPollEvents();
        if (vk_ != nullptr) {
            (void)vk_->draw_frame();
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds {16});
        }
        if (useAutoClose) {
            const auto now {std::chrono::steady_clock::now()};
            if (now - startTime >= autoCloseMs) {
                glfwSetWindowShouldClose(window_, GLFW_TRUE);
            }
        }
    }
}

} // namespace vulkano
