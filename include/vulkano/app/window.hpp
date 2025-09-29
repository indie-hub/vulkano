#pragma once

#include <cstdint>
#include <glm/vec2.hpp>
#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace vulkano::app {
class Window final {
public:
    Window(const char* title, std::uint32_t width, std::uint32_t height);
    ~Window() noexcept;

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) noexcept = delete;
    Window& operator=(Window&&) noexcept = delete;

    [[nodiscard]] GLFWwindow* handle() const noexcept;
    [[nodiscard]] bool should_close() const noexcept;
    void poll_events() const noexcept;
    [[nodiscard]] VkExtent2D framebuffer_extent() const noexcept;
    [[nodiscard]] bool framebuffer_resized() const noexcept;
    void clear_framebuffer_resized() noexcept;
    void mark_framebuffer_resized() noexcept;
    void on_cursor_moved(double xpos, double ypos) noexcept;

    [[nodiscard]] bool is_key_pressed(int key) const noexcept;
    [[nodiscard]] bool is_mouse_button_pressed(int button) const noexcept;
    void set_cursor_capture(bool capture) noexcept;
    void reset_cursor_delta() noexcept;
    [[nodiscard]] glm::vec2 consume_cursor_delta() noexcept;

private:
    GLFWwindow* m_window {nullptr};
    bool m_framebufferResized {false};
    bool m_cursorCaptured {false};
    bool m_firstCursorCapture {true};
    glm::vec2 m_cursorDelta {0.0F, 0.0F};
    double m_lastCursorX {0.0};
    double m_lastCursorY {0.0};
};
} // namespace vulkano::app
