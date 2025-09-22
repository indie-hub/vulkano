#include <vulkano/glfw_context.hpp>

#include <stdexcept>
#include <cstdio>

namespace {
    void error_callback(int code, const char* description) {
        if(description == nullptr) {
            return;
        }
        // Log to stderr; future logging system can capture this.
        std::fprintf(stderr, "GLFW error (%d): %s\n", code, description);
    }
} // namespace

namespace vulkano {

GlfwContext::GlfwContext() {
    if(glfwInit() != GLFW_TRUE) {
        throw std::runtime_error {"Failed to initialise GLFW"};
    }
    glfwSetErrorCallback(&error_callback);
}

GlfwContext::~GlfwContext() noexcept {
    glfwTerminate();
}

void GlfwContext::configure_hints(const AppConfig& config) const noexcept {
    (void)config;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
}

auto GlfwContext::create_window(const AppConfig& config) const -> GLFWwindow* {
    configure_hints(config);
    const auto extent = config.window_extent();
    GLFWwindow* window = glfwCreateWindow(static_cast<int>(extent.width), static_cast<int>(extent.height), config.application_name().c_str(), nullptr, nullptr);
    if(window == nullptr) {
        throw std::runtime_error {"Failed to create GLFW window"};
    }
    return window;
}

} // namespace vulkano
