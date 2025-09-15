#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace vulkano {

class WindowImpl;

class Window final {
public:
    Window() = delete;
    explicit Window(const std::string& title, std::uint32_t width, std::uint32_t height);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept;

    void poll_events() const noexcept;
    bool should_close() const noexcept;
    void set_title(const std::string& title) noexcept;

    void* native_handle() const noexcept;

private:
    std::unique_ptr<WindowImpl> impl;
};

} // namespace vulkano

