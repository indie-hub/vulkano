// window.hpp
#pragma once
#include <cstdint>
#include <string>

struct GLFWwindow;

namespace vulkano {

struct WindowConfig final {
    std::string title {};
    std::uint32_t width {1280U};
    std::uint32_t height {720U};
    bool resizable {true};
};

class Window final {
public:
    Window() = delete;
    explicit Window(const WindowConfig& config);
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept;
    ~Window();

    [[nodiscard]] GLFWwindow* handle() const noexcept;
    [[nodiscard]] std::uint32_t width() const noexcept;
    [[nodiscard]] std::uint32_t height() const noexcept;
    [[nodiscard]] bool should_close() const noexcept;
    void poll_events() const noexcept;
    void wait_events() const noexcept;
    void set_framebuffer_resized(bool resized) noexcept;
    [[nodiscard]] bool framebuffer_resized() const noexcept;

private:
    GLFWwindow* m_window {nullptr};
    std::uint32_t m_width {0U};
    std::uint32_t m_height {0U};
    bool m_framebuffer_resized {false};
};

} // namespace vulkano

