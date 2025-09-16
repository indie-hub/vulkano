// window.cpp
#include <vulkano/window.hpp>

#include <GLFW/glfw3.h>
#include <stdexcept>

namespace vulkano {

namespace {
    void framebuffer_size_callback(GLFWwindow* window, int width, int height) noexcept {
        (void)width;
        (void)height;
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (self != nullptr) {
            self->set_framebuffer_resized(true);
        }
    }
}

Window::Window(const WindowConfig& config) : m_window {nullptr}, m_width {config.width}, m_height {config.height}, m_framebuffer_resized {false} {
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error{"Failed to initialize GLFW"};
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

    m_window = glfwCreateWindow(static_cast<int>(config.width), static_cast<int>(config.height), config.title.c_str(), nullptr, nullptr);
    if (m_window == nullptr) {
        glfwTerminate();
        throw std::runtime_error{"Failed to create GLFW window"};
    }
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebuffer_size_callback);
}

Window::Window(Window&& other) noexcept {
    m_window = other.m_window;
    m_width = other.m_width;
    m_height = other.m_height;
    m_framebuffer_resized = other.m_framebuffer_resized;
    other.m_window = nullptr;
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        if (m_window != nullptr) {
            glfwDestroyWindow(m_window);
            glfwTerminate();
        }
        m_window = other.m_window;
        m_width = other.m_width;
        m_height = other.m_height;
        m_framebuffer_resized = other.m_framebuffer_resized;
        other.m_window = nullptr;
    }
    return *this;
}

Window::~Window() {
    if (m_window != nullptr) {
        glfwDestroyWindow(m_window);
        glfwTerminate();
        m_window = nullptr;
    }
}

GLFWwindow* Window::handle() const noexcept {
    return m_window;
}

std::uint32_t Window::width() const noexcept { return m_width; }
std::uint32_t Window::height() const noexcept { return m_height; }

bool Window::should_close() const noexcept {
    return glfwWindowShouldClose(m_window) == GLFW_TRUE;
}

void Window::poll_events() const noexcept {
    glfwPollEvents();
}

void Window::wait_events() const noexcept {
    glfwWaitEvents();
}

void Window::set_framebuffer_resized(bool resized) noexcept {
    m_framebuffer_resized = resized;
}

bool Window::framebuffer_resized() const noexcept {
    return m_framebuffer_resized;
}

} // namespace vulkano

