#include <vulkano/app.hpp>
#include <vulkano/vulkan_context.hpp>

#include <chrono>
#include <cstdlib>
#include <thread>
#include <imgui.h>

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
    if (window_ != nullptr) {
        glfwSetWindowUserPointer(window_, this);
        glfwSetFramebufferSizeCallback(window_, &App::framebuffer_size_callback);
    }
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
            // Begin ImGui frame and build minimal overlay
            vk_->imgui_new_frame();
            build_ui();
            const bool ok {(vk_->draw_frame())};
            if (!ok || framebuffer_resized_) {
                int width {0};
                int height {0};
                do {
                    glfwGetFramebufferSize(window_, &width, &height);
                    if (width == 0 || height == 0) {
                        glfwWaitEvents();
                    }
                } while (width == 0 || height == 0);

                vk_->recreate_swapchain(window_);
                framebuffer_resized_ = false;
            }
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

void App::framebuffer_size_callback(GLFWwindow* window, int width, int height) noexcept {
    (void)width;
    (void)height;
    if (window == nullptr) {
        return;
    }
    auto* appPtr {static_cast<App*>(glfwGetWindowUserPointer(window))};
    if (appPtr != nullptr) {
        appPtr->framebuffer_resized_ = true;
    }
}

void App::build_ui() noexcept {
    ImGui::SetNextWindowBgAlpha(0.4F);
    ImGui::Begin("Overlay", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
    ImGui::Text("ImGui initialized");
    ImGui::End();
}

} // namespace vulkano
