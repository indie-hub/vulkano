#include <vulkano/app/window.hpp>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdint>
#include <glm/vec2.hpp>
#include <stdexcept>

namespace {
void framebuffer_size_callback(GLFWwindow* window, int /*width*/, int /*height*/) {
    auto* userdata = static_cast<vulkano::app::Window*>(glfwGetWindowUserPointer(window));
    if (userdata != nullptr) {
        userdata->mark_framebuffer_resized();
    }
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    auto* userdata = static_cast<vulkano::app::Window*>(glfwGetWindowUserPointer(window));
    if (userdata != nullptr) {
        userdata->on_cursor_moved(xpos, ypos);
    }
}

GLFWwindow* create_window(const char* title, std::uint32_t width, std::uint32_t height) {
    if (title == nullptr) {
        throw std::runtime_error {"Window title must not be null"};
    }
    if (width == 0U || height == 0U) {
        throw std::runtime_error {"Window dimensions must be greater than zero"};
    }

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), title, nullptr, nullptr);
    if (window == nullptr) {
        throw std::runtime_error {"Failed to create GLFW window"};
    }

    return window;
}
} // namespace

namespace vulkano::app {
Window::Window(const char* title, std::uint32_t width, std::uint32_t height)
    : m_window {create_window(title, width, height)} {
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebuffer_size_callback);
    glfwSetCursorPosCallback(m_window, cursor_position_callback);
}

Window::~Window() noexcept {
    if (m_window != nullptr) {
        glfwSetCursorPosCallback(m_window, nullptr);
        glfwSetWindowUserPointer(m_window, nullptr);
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
}

GLFWwindow* Window::handle() const noexcept {
    return m_window;
}

bool Window::should_close() const noexcept {
    if (m_window == nullptr) {
        return true;
    }
    return glfwWindowShouldClose(m_window) == GLFW_TRUE;
}

void Window::poll_events() const noexcept {
    glfwPollEvents();
}

VkExtent2D Window::framebuffer_extent() const noexcept {
    if (m_window == nullptr) {
        return VkExtent2D {0U, 0U};
    }
    int width {0};
    int height {0};
    glfwGetFramebufferSize(m_window, &width, &height);
    const std::uint32_t clampedWidth {static_cast<std::uint32_t>(std::max(width, 0))};
    const std::uint32_t clampedHeight {static_cast<std::uint32_t>(std::max(height, 0))};
    return VkExtent2D {clampedWidth, clampedHeight};
}

bool Window::framebuffer_resized() const noexcept {
    return m_framebufferResized;
}

void Window::clear_framebuffer_resized() noexcept {
    m_framebufferResized = false;
}

void Window::mark_framebuffer_resized() noexcept {
    m_framebufferResized = true;
}

bool Window::is_key_pressed(int key) const noexcept {
    if (m_window == nullptr) {
        return false;
    }
    return glfwGetKey(m_window, key) == GLFW_PRESS;
}

bool Window::is_mouse_button_pressed(int button) const noexcept {
    if (m_window == nullptr) {
        return false;
    }
    return glfwGetMouseButton(m_window, button) == GLFW_PRESS;
}

void Window::set_cursor_capture(bool capture) noexcept {
    if (m_window == nullptr) {
        return;
    }
    m_cursorCaptured = capture;
    m_firstCursorCapture = true;
    m_cursorDelta = glm::vec2 {0.0F, 0.0F};
    glfwSetInputMode(m_window, GLFW_CURSOR, capture ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void Window::reset_cursor_delta() noexcept {
    m_cursorDelta = glm::vec2 {0.0F, 0.0F};
}

glm::vec2 Window::consume_cursor_delta() noexcept {
    const glm::vec2 delta = m_cursorDelta;
    m_cursorDelta = glm::vec2 {0.0F, 0.0F};
    return delta;
}

void Window::on_cursor_moved(double xpos, double ypos) noexcept {
    if (!m_cursorCaptured) {
        m_lastCursorX = xpos;
        m_lastCursorY = ypos;
        m_firstCursorCapture = true;
        return;
    }
    if (m_firstCursorCapture) {
        m_lastCursorX = xpos;
        m_lastCursorY = ypos;
        m_firstCursorCapture = false;
        m_cursorDelta = glm::vec2 {0.0F, 0.0F};
        return;
    }
    m_cursorDelta.x += static_cast<float>(xpos - m_lastCursorX);
    m_cursorDelta.y += static_cast<float>(m_lastCursorY - ypos);
    m_lastCursorX = xpos;
    m_lastCursorY = ypos;
}
} // namespace vulkano::app
