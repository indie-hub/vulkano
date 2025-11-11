#pragma once

#include <cstdint>
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

private:
    GLFWwindow* m_window {nullptr};
    bool m_framebufferResized {false};
};
} // namespace vulkano::app
